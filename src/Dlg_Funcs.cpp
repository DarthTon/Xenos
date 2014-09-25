#include "MainWnd.h"

#include <shellapi.h>

INT_PTR MainDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    Dialog::OnInit( hDlg, message, wParam, lParam );

    //
    // Setup controls
    //
    _procList.Attach( GetDlgItem( _hwnd, IDC_COMBO_PROC ) );
    _threadList.Attach( GetDlgItem( _hwnd, IDC_THREADS ) );
    _injectionType.Attach( GetDlgItem( _hwnd, IDC_OP_TYPE ) );
    _initFuncList.Attach( GetDlgItem( _hwnd, IDC_INIT_EXPORT ) );

    _imagePath.Attach( GetDlgItem( _hwnd, IDC_IMAGE_PATH ) );
    _procCmdLine.Attach( GetDlgItem( _hwnd, IDC_CMDLINE ) );
    _initArg.Attach( GetDlgItem( _hwnd, IDC_ARGUMENT ) );

    _exProc.Attach( GetDlgItem( _hwnd, IDC_EXISTING_PROC ) );
    _newProc.Attach( GetDlgItem( _hwnd, IDC_NEW_PROC ) );
    _autoProc.Attach( GetDlgItem( _hwnd, IDC_AUTO_PROC ) );

    _selectProc.Attach( GetDlgItem( _hwnd, IDC_SELECT_PROC ) );
    _injClose.Attach( GetDlgItem( _hwnd, IDC_INJ_CLOSE ) );
    _unlink.Attach( GetDlgItem( _hwnd, IDC_UNLINK ) );

    _mmapOptions.addLdrRef.Attach( GetDlgItem( _hwnd, IDC_LDR_REF ) );
    _mmapOptions.manualInmport.Attach( GetDlgItem( _hwnd, IDC_MANUAL_IMP ) );
    _mmapOptions.noTls.Attach( GetDlgItem( _hwnd, IDC_IGNORE_TLS ) );
    _mmapOptions.noExceptions.Attach( GetDlgItem( _hwnd, IDC_NOEXCEPT ) );
    _mmapOptions.wipeHeader.Attach( GetDlgItem( _hwnd, IDC_WIPE_HDR ) );
    _mmapOptions.hideVad.Attach( GetDlgItem( _hwnd, IDC_HIDEVAD ) );

    _mmapOptions.hideVad.disable();

    // Set dialog title
    SetWindowTextW( _hwnd, blackbone::Utils::RandomANString().c_str() );

    // Fill injection types
    _injectionType.Add( L"Native inject", Normal );
    _injectionType.Add( L"Manual map", Manual );

    // Disable kernel injection if no driver available
    if (blackbone::Driver().loaded())
    {
        _injectionType.Add( L"Kernel (CreateThread)", Kernel_Thread );
        _injectionType.Add( L"Kernel (APC)", Kernel_APC );
        _injectionType.Add( L"Kernel driver manual map", Kernel_DriverMap);

        _mmapOptions.hideVad.enable();

        EnableMenuItem( GetMenu( _hwnd ), ID_TOOLS_PROTECT, MF_ENABLED );
    }

    _injectionType.selection( 0 );

    _mmapOptions.addLdrRef.disable();
    _mmapOptions.manualInmport.disable();
    _mmapOptions.noTls.disable();
    _mmapOptions.noExceptions.disable();
    _mmapOptions.wipeHeader.disable();

    // Set icon
    HICON hIcon = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_ICON1 ) );
    SendMessage( hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
    SendMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
    DestroyIcon( hIcon );

    LoadConfig();

    return TRUE;
}


INT_PTR MainDlg::OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    SaveConfig();
    return Dialog::OnClose( hDlg, message, wParam, lParam );
}


//////////////////////////////////////////////////////////////////////////////////////

INT_PTR MainDlg::OnFileExit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    EndDialog( hDlg, 0 );
    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnDropDown( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (LOWORD( wParam ) == IDC_COMBO_PROC)
        FillProcessList();

    return TRUE;
}

INT_PTR MainDlg::OnSelChange( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    // Process selection
    if (LOWORD( wParam ) == IDC_COMBO_PROC)
    {
        DWORD pid   = (DWORD)_procList.itemData( _procList.selection() );
        auto opType = _injectionType.selection();
        auto path   = _procList.selectedText().substr( 0, _procList.selectedText().find( L'(' ) - 1 );

        SetActiveProcess( pid, path );
    }
    // Injection type selection
    else if (LOWORD( wParam ) == IDC_OP_TYPE)
    {
        UpdateInterface( (MapMode)_injectionType.selection() );
    }

    return TRUE;
}


INT_PTR MainDlg::OnLoadImage( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    OPENFILENAMEW ofn = { 0 };
    wchar_t path[MAX_PATH] = { 0 };
    BOOL bSucc = FALSE;

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT( "All (*.*)\0*.*\0Dynamic link library (*.dll)\0*.dll\0System driver (*.sys)\0*.sys\0" );
    ofn.nFilterIndex = _injectionType.selection() == Kernel_DriverMap ? 3 : 2;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST;

    bSucc = GetOpenFileName( &ofn );
    if (bSucc)
    {
        blackbone::pe::listExports exports;

        if (_core.LoadImageFile( path, exports ) == ERROR_SUCCESS)
        {
            _imagePath.text( path );

            _initFuncList.reset();
            for (auto& exprt : exports)
                _initFuncList.Add( exprt.first );
        }
        else
            _imagePath.reset();
    }

    return TRUE;
}

INT_PTR MainDlg::OnExecute( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    InjectContext ctx;

    ctx.flags = MmapFlags();
    ctx.imagePath = _imagePath.text();
    ctx.initRoutine = blackbone::Utils::WstringToAnsi( _initFuncList.selectedText() );
    ctx.initRoutineArg = _initArg.text();
    ctx.procMode = (_exProc.checked() ? Existing : (_newProc.checked() ? NewProcess : ManualLaunch));
    ctx.injectMode = (MapMode)_injectionType.selection();
    ctx.pid = _procList.itemData( _procList.selection() );
    ctx.procCmdLine = _procCmdLine.text();
    ctx.procPath = _processPath;
    ctx.threadID = _threadList.itemData( _threadList.selection() );
    ctx.unlinkImage = _unlink;

    _core.DoInject( ctx );

    // Show wait dialog
    if (ctx.procMode == ManualLaunch)
    {
        DlgWait dlgWait( _core );
        dlgWait.RunModal( hDlg );
    }

    // Close after successful injection
    if (_injClose.checked())
    {
        auto closeRoutine = []( LPVOID lpParam ) -> DWORD
        {  
            auto pDlg = (MainDlg*)lpParam;
            if (pDlg->_core.WaitOnInjection() == ERROR_SUCCESS)
            {
                pDlg->SaveConfig();
                ::EndDialog( pDlg->_hwnd, 0 );
            }

            return 0;
        };

        CreateThread( NULL, 0, static_cast<LPTHREAD_START_ROUTINE>(closeRoutine), this, 0, NULL );
    }

    return TRUE;
}

INT_PTR MainDlg::OnExistingProcess( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    UpdateInterface( (MapMode)_injectionType.selection() );
    return TRUE;
}

INT_PTR MainDlg::OnSelectExecutable( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    std::wstring path;
    if (SelectExecutable( path ))
        SetActiveProcess( 0, path );

    UpdateInterface( (MapMode)_injectionType.selection() );

    return TRUE;
}

INT_PTR MainDlg::OnDragDrop( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    wchar_t path[MAX_PATH] = { 0 };
    HDROP hDrop = (HDROP)wParam;

    if(DragQueryFile( hDrop, 0, path, ARRAYSIZE( path ) ) != 0)
    {
        blackbone::pe::listExports exports;

        if (_core.LoadImageFile( path, exports ) == ERROR_SUCCESS)
        {
            _imagePath.text( path );

            _initFuncList.reset();
            for (auto& exprt : exports)
                _initFuncList.Add( exprt.first );
        }
        else
            _imagePath.reset();
    }

    return TRUE;
}

INT_PTR MainDlg::OnProtectSelf( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    NTSTATUS status = STATUS_SUCCESS;

    // Protect current process
    if (blackbone::Driver().loaded())
        status = blackbone::Driver().ProtectProcess( GetCurrentProcessId(), true );

    if (!NT_SUCCESS( status ))
        Message::ShowError( hDlg, L"Failed to protect current process.\n" + blackbone::Utils::GetErrorDescription( status ) );
    else
        Message::ShowInfo( hDlg, L"Successfully protected" );

    return TRUE;
}


INT_PTR MainDlg::OnEjectModules( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    auto pid = _procList.itemData( _procList.selection() );

    // Try attaching to current selection
    if (_core.process().pid() != pid)
    {
        auto status = _core.process().Attach( pid );
        if (status != STATUS_SUCCESS)
        {
            std::wstring errmsg = L"Can not attach to process.\n" + blackbone::Utils::GetErrorDescription( status );
            Message::ShowError( hDlg, errmsg );
        }
    }
    
    if (!_core.process().valid())
    {
        Message::ShowError( hDlg, L"Please select valid process before unloading modules" );
        return TRUE;
    }

    ModulesDlg dlg( _core.process() );
    return dlg.RunModal( hDlg );
}
