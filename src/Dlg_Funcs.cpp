#include "MainWnd.h"

#include <shellapi.h>

INT_PTR MainDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _hMainDlg = hDlg;

    // Set dialog title
    SetRandomTitle();
/*#ifdef _M_AMD64
    SetWindowText( hDlg, L"Xenos64" );
#endif*/

    // Fill inject methods
    HWND hOpTypeList = GetDlgItem( hDlg, IDC_OP_TYPE );

    ComboBox_AddString( hOpTypeList, L"Native inject" );
    ComboBox_AddString( hOpTypeList, L"Manual map" );
    ComboBox_SetCurSel( hOpTypeList, 0 );

    EnableWindow( GetDlgItem( hDlg, IDC_CMDLINE ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_WIPE_HDR ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_LDR_REF ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_MANUAL_IMP ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_IGNORE_TLS ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_NOEXCEPT), FALSE );

    // Set icon
    HICON hIcon = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_ICON1 ) );

    SendMessage( hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
    SendMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );

    ConfigMgr::ConfigData cfg;
    if (_config.Load( cfg ))
    {
        // Image
        if (!cfg.imagePath.empty())
        {
            Edit_SetText( GetDlgItem( hDlg, IDC_IMAGE_PATH ), cfg.imagePath.c_str() );
            LoadImageFile( cfg.imagePath.c_str() );
        }

        // Process
        if (!cfg.procName.empty())
        {
            DWORD pid = 0xFFFFFFFF;

            if (!cfg.newProcess && !cfg.procName.empty())
            {
                std::vector<DWORD> procList;
                blackbone::Process::EnumByName( cfg.procName, procList );
                pid = !procList.empty() ? procList.front() : 0xFFFFFFFF;
            }

            SetActiveProcess( cfg.newProcess, cfg.procName.c_str(), pid );
        }

        if (!cfg.procCmdLine.empty())
            SetDlgItemTextW( _hMainDlg, IDC_CMDLINE, cfg.procCmdLine.c_str() );

        if (!cfg.initArgs.empty())
            SetDlgItemTextW( _hMainDlg, IDC_ARGUMENT, cfg.initArgs.c_str() );

        if (!cfg.initRoutine.empty())
            SetDlgItemTextW( _hMainDlg, IDC_INIT_EXPORT, cfg.initRoutine.c_str() );

        if (cfg.unlink)
            Button_SetCheck( GetDlgItem( _hMainDlg, IDC_UNLINK ), TRUE );

        if (cfg.manualMap)
            ComboBox_SetCurSel( GetDlgItem( _hMainDlg, IDC_OP_TYPE ), 1 );

        SetMapMode( cfg.manualMap ? Manual : Normal );
        MmapFlags( cfg.manualMapFlags );
    }    

    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (Events.count( LOWORD( wParam ) ))
        return (this->*Events[LOWORD( wParam )])(hDlg, message, wParam, lParam);

    if (Events.count( HIWORD( wParam ) ))
        return (this->*Events[HIWORD( wParam )])(hDlg, message, wParam, lParam);
    
    return (INT_PTR)FALSE;
}

INT_PTR MainDlg::OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ConfigMgr::ConfigData cfg;
    wchar_t imagePath[MAX_PATH];
    wchar_t initRoutine[MAX_PATH];
    wchar_t initArgs[MAX_PATH];
    wchar_t procCmdLine[MAX_PATH];

    cfg.newProcess = _newProcess;
    cfg.procName = _procPath;

    GetDlgItemTextW( _hMainDlg, IDC_IMAGE_PATH,  imagePath, ARRAYSIZE( imagePath ) );
    GetDlgItemTextW( _hMainDlg, IDC_CMDLINE,     procCmdLine, ARRAYSIZE( procCmdLine ) );
    GetDlgItemTextW( _hMainDlg, IDC_ARGUMENT,    initArgs, ARRAYSIZE( initArgs ) );
    GetDlgItemTextW( _hMainDlg, IDC_INIT_EXPORT, initRoutine, ARRAYSIZE( initRoutine ) );

    HWND hCombo = GetDlgItem( _hMainDlg, IDC_THREADS );
    DWORD thdId = (DWORD)ComboBox_GetItemData( hCombo, ComboBox_GetCurSel( hCombo ) );

    cfg.imagePath   = imagePath;
    cfg.procCmdLine = procCmdLine;
    cfg.initRoutine = initRoutine;
    cfg.initArgs    = initArgs;
    cfg.imagePath   = imagePath;

    cfg.manualMap = (ComboBox_GetCurSel( GetDlgItem( hDlg, IDC_OP_TYPE ) ) == 1);
    cfg.threadHijack = (thdId != 0 && thdId != 0xFFFFFFFF);
    cfg.manualMapFlags = MmapFlags();
    cfg.unlink = Button_GetCheck( GetDlgItem( _hMainDlg, IDC_UNLINK ) ) == 0 ? false : true;

    _config.Save( cfg );

    EndDialog( hDlg, 0 );
    return (INT_PTR)TRUE;
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

    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnSelChange( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (LOWORD( wParam ) == IDC_COMBO_PROC)
    {
        HWND hCombo = GetDlgItem( hDlg, LOWORD( wParam ) );
        DWORD pid   = (DWORD)ComboBox_GetItemData( hCombo, ComboBox_GetCurSel( hCombo ) );

        SetActiveProcess( false, nullptr, pid );
    }
    // Update available options
    else if (LOWORD( wParam ) == IDC_OP_TYPE)
    {
        int idx = ComboBox_GetCurSel( GetDlgItem( hDlg, LOWORD( wParam ) ) );
        SetMapMode( idx == 0 ? Normal : Manual );
    }

    return (INT_PTR)TRUE;
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
        Edit_SetText( GetDlgItem( hDlg, IDC_IMAGE_PATH ), path );
        LoadImageFile( path );
    }

    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnExecute( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    wchar_t path[MAX_PATH] = { 0 };
    char init[256] = { 0 };
    wchar_t arg[512] = { 0 };

    GetDlgItemTextW( hDlg, IDC_IMAGE_PATH, path, MAX_PATH );
    GetDlgItemTextA( hDlg, IDC_INIT_EXPORT, init, 256 );
    GetDlgItemTextW( hDlg, IDC_ARGUMENT, arg, 512 );

    DoInject( path, init, arg );

    return (INT_PTR)TRUE;
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
        SetActiveProcess( true, ofn.lpstrFile );

    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnDragDrop( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    wchar_t path[MAX_PATH] = { 0 };
    HDROP hDrop = (HDROP)wParam;

    if(DragQueryFile( hDrop, 0, path, ARRAYSIZE( path ) ) != 0)
    {
        Edit_SetText( GetDlgItem( hDlg, IDC_IMAGE_PATH ), path );
        LoadImageFile( path );
    }

    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnEjectModules( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (!_proc.valid())
    {
        MessageBoxW( hDlg, L"Please select valid process before unloading modules", L"Error", MB_ICONERROR );
        return TRUE;
    }

    return DialogBox( GetModuleHandle( NULL ), MAKEINTRESOURCEW( IDD_MODULES ), _hMainDlg, &ModulesDlg::DlgProcModules );
}
