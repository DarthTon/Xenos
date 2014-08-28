#include "MainWnd.h"

#include <shellapi.h>

INT_PTR MainDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _hMainDlg = hDlg;
    blackbone::Process thisProc;
    thisProc.Attach( GetCurrentProcessId() );

    //
    // Setup controls
    //
    _procList.Attach( GetDlgItem( _hMainDlg, IDC_COMBO_PROC ) );
    _threadList.Attach( GetDlgItem( _hMainDlg, IDC_THREADS ) );
    _injectionType.Attach( GetDlgItem( _hMainDlg, IDC_OP_TYPE ) );
    _initFuncList.Attach( GetDlgItem( _hMainDlg, IDC_INIT_EXPORT ) );

    _imagePath.Attach( GetDlgItem( _hMainDlg, IDC_IMAGE_PATH ) );
    _procCmdLine.Attach( GetDlgItem( _hMainDlg, IDC_CMDLINE ) );
    _initArg.Attach( GetDlgItem( _hMainDlg, IDC_ARGUMENT ) );

    _unlink.Attach( GetDlgItem( _hMainDlg, IDC_UNLINK ) );

    _mmapOptions.addLdrRef.Attach( GetDlgItem( _hMainDlg, IDC_LDR_REF ) );
    _mmapOptions.manualInmport.Attach( GetDlgItem( _hMainDlg, IDC_MANUAL_IMP ) );
    _mmapOptions.noTls.Attach( GetDlgItem( _hMainDlg, IDC_IGNORE_TLS ) );
    _mmapOptions.noExceptions.Attach( GetDlgItem( _hMainDlg, IDC_NOEXCEPT ) );
    _mmapOptions.wipeHeader.Attach( GetDlgItem( _hMainDlg, IDC_WIPE_HDR ) );

    // Set dialog title
    SetWindowTextW( _hMainDlg, blackbone::Utils::RandomANString().c_str() );

    // Fill injection types
    _injectionType.Add( L"Native inject", Normal );
    _injectionType.Add( L"Manual map", Manual );

    // Disable kernel injection for x86 OS
    if (!thisProc.core().native()->GetWow64Barrier().x86OS)
    {
        _injectionType.Add( L"Kernel (CreateThread)", Kernel_Thread );
        _injectionType.Add( L"Kernel (APC)", Kernel_APC );
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

INT_PTR MainDlg::OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (_events.count( LOWORD( wParam ) ))
        return (this->*_events[LOWORD( wParam )])(hDlg, message, wParam, lParam);

    if (_events.count( HIWORD( wParam ) ))
        return (this->*_events[HIWORD( wParam )])(hDlg, message, wParam, lParam);
    
    return FALSE;
}

INT_PTR MainDlg::OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    //
    // SaveConfig
    //
    SaveConfig();


    return EndDialog( hDlg, 0 );
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
        SetMapMode( (MapMode)_injectionType.selection() );
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
    ofn.lpstrFilter = TEXT( "All (*.*)\0*.*\0Dynamic link library (*.dll)\0*.dll\0" );
    ofn.nFilterIndex = 2;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST;

    bSucc = GetOpenFileName( &ofn );
    if (bSucc)
    {
        std::list<std::string> exportNames;

        if (_core.LoadImageFile( path, exportNames ) == ERROR_SUCCESS)
        {
            _imagePath.text( path );

            _initFuncList.reset();
            for (auto& name : exportNames)
                _initFuncList.Add( name );
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
    ctx.mode = (MapMode)_injectionType.selection();
    ctx.pid = _procList.itemData( _procList.selection() );
    ctx.procCmdLine = _procCmdLine.text();
    ctx.procPath = _processPath;
    ctx.threadID = _threadList.itemData( _threadList.selection() );
    ctx.unlinkImage = _unlink;

    _core.DoInject( ctx );

    return TRUE;
}

INT_PTR MainDlg::OnNewProcess( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    OPENFILENAMEW ofn = { 0 };
    wchar_t path[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT( "All (*.*)\0*.*\0Executable image (*.exe)\0*.exe\0" );
    ofn.nFilterIndex = 2;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST;

    if (GetOpenFileName( &ofn ))
        SetActiveProcess( 0, ofn.lpstrFile );

    return TRUE;
}

INT_PTR MainDlg::OnDragDrop( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    wchar_t path[MAX_PATH] = { 0 };
    HDROP hDrop = (HDROP)wParam;

    if(DragQueryFile( hDrop, 0, path, ARRAYSIZE( path ) ) != 0)
    {
        std::list<std::string> exportNames;

        if (_core.LoadImageFile( path, exportNames ) == ERROR_SUCCESS)
        {
            _imagePath.text( path );

            _initFuncList.reset();
            for (auto& name : exportNames)
                _initFuncList.Add( name );
        }
        else
            _imagePath.reset();
    }

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

    return DialogBox( GetModuleHandle( NULL ), MAKEINTRESOURCEW( IDD_MODULES ), _hMainDlg, &ModulesDlg::DlgProcModules );
}
