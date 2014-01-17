#include "MainWnd.h"

#include <shellapi.h>

INT_PTR MainDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _hMainDlg = hDlg;

    // Set dialog title
#ifdef _M_AMD64
    SetWindowText( hDlg, L"Xenos64" );
#endif

    // Fill inject methods
    HWND hCombo = GetDlgItem( hDlg, IDC_OP_TYPE );

    ComboBox_AddString( hCombo, L"Native inject" );
    ComboBox_AddString( hCombo, L"Manual map" );
    ComboBox_SetCurSel( hCombo, 0 );

    EnableWindow( GetDlgItem( hDlg, IDC_WIPE_HDR ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_LDR_REF ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_MANUAL_IMP ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_IGNORE_TLS ), FALSE );
    EnableWindow( GetDlgItem( hDlg, IDC_NOEXCEPT), FALSE );

    // Set icon
    HICON hIcon = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_ICON1 ) );

    SendMessage( hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
    SendMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );

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
        AttachToProcess( pid );
        FillThreads();

        _newProcess = false;
    }
    else if (LOWORD( wParam ) == IDC_OP_TYPE)
    {
        HWND hCombo = GetDlgItem( hDlg, LOWORD( wParam ) );

        int idx = ComboBox_GetCurSel( hCombo );

        // Native inject
        if (idx == 0)
        {
            EnableWindow( GetDlgItem( hDlg, IDC_THREADS ), TRUE );
            EnableWindow( GetDlgItem( hDlg, IDC_UNLINK ), TRUE );

            EnableWindow( GetDlgItem( hDlg, IDC_WIPE_HDR ), FALSE );
            EnableWindow( GetDlgItem( hDlg, IDC_LDR_REF ), FALSE );
            EnableWindow( GetDlgItem( hDlg, IDC_MANUAL_IMP ), FALSE );
            EnableWindow( GetDlgItem( hDlg, IDC_IGNORE_TLS ), FALSE );
            EnableWindow( GetDlgItem( hDlg, IDC_NOEXCEPT ), FALSE );
        }
        // Manual map
        else if (idx == 1)
        {
            ComboBox_SetCurSel( GetDlgItem( hDlg, IDC_THREADS ), 0 );
            EnableWindow( GetDlgItem( hDlg, IDC_THREADS ), FALSE );
            EnableWindow( GetDlgItem( hDlg, IDC_UNLINK ), FALSE );

            EnableWindow( GetDlgItem( hDlg, IDC_WIPE_HDR ), TRUE );
            EnableWindow( GetDlgItem( hDlg, IDC_LDR_REF ), TRUE );
            EnableWindow( GetDlgItem( hDlg, IDC_MANUAL_IMP ), TRUE );
            EnableWindow( GetDlgItem( hDlg, IDC_IGNORE_TLS ), TRUE );
            EnableWindow( GetDlgItem( hDlg, IDC_NOEXCEPT ), TRUE );
        }
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
        LoadImage( path );
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
    BOOL bSucc = FALSE;

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

    bSucc = GetOpenFileName( &ofn );
    if (bSucc)
    {
        std::wstring procName = blackbone::Utils::StripPath( ofn.lpstrFile ) + L" (New process)";
        HWND hCombo = GetDlgItem( hDlg, IDC_COMBO_PROC );

        auto idx = ComboBox_AddString( hCombo, procName.c_str( ) );
        ComboBox_SetItemData( hCombo, idx, -1 );
        ComboBox_SetCurSel( hCombo, idx );

        _newProcess = true;
        _procPath = ofn.lpstrFile;
    }

    return (INT_PTR)TRUE;
}

INT_PTR MainDlg::OnDragDrop( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    wchar_t path[MAX_PATH] = { 0 };
    HDROP hDrop = (HDROP)wParam;

    if(DragQueryFile( hDrop, 0, path, ARRAYSIZE( path ) ) != 0)
    {
        Edit_SetText( GetDlgItem( hDlg, IDC_IMAGE_PATH ), path );
        LoadImage( path );
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
