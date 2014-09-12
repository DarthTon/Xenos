#include "stdafx.h"
#include "MainWnd.h"

MainDlg::MainDlg()
    : _hMainDlg( 0 )
    , _modulesDlg( _core.process() )
    , _core( _hMainDlg )
{
}

MainDlg::~MainDlg()
{
}

/// <summary>
/// Get singleton instance
/// </summary>
/// <returns>Main dialog instance</returns>
MainDlg& MainDlg::Instance()
{
    static MainDlg ms_Instanse;
    return ms_Instanse;
}

/// <summary>
/// Run GUI
/// </summary>
/// <returns></returns>
INT_PTR MainDlg::Run()
{
    _messages[WM_INITDIALOG] = &MainDlg::OnInit;
    _messages[WM_COMMAND]    = &MainDlg::OnCommand;
    _messages[WM_CLOSE]      = &MainDlg::OnClose;
    _messages[WM_DROPFILES]  = &MainDlg::OnDragDrop;

    _events[IDC_EXECUTE]     = &MainDlg::OnExecute;
    _events[IDC_NEW_PROC]    = &MainDlg::OnNewProcess;
    _events[IDC_LOAD_IMG]    = &MainDlg::OnLoadImage;
    _events[CBN_DROPDOWN]    = &MainDlg::OnDropDown;
    _events[CBN_SELCHANGE]   = &MainDlg::OnSelChange;

    _events[ID_TOOLS_PROTECT]      = &MainDlg::OnProtectSelf;
    _events[ID_TOOLS_EJECTMODULES] = &MainDlg::OnEjectModules;

    return DialogBox( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDD_MAIN ), NULL, &MainDlg::DlgProcMain );
}

/// <summary>
/// Dialog message handler
/// </summary>
INT_PTR CALLBACK MainDlg::DlgProcMain( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    auto& inst = MainDlg::Instance();
    if (inst._messages.find( message ) != inst._messages.end())
        return (inst.*inst._messages[message])(hDlg, message, wParam, lParam);

    return FALSE;
}


