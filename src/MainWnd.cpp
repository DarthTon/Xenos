#include "stdafx.h"
#include "MainWnd.h"

MainDlg::mapMsgProc MainDlg::Messages;


MainDlg::MainDlg()
    : _hMainDlg( 0 )
    , _hPropSheet( 0 )
    , _newProcess( false )
    , _proc()
{
}

MainDlg::~MainDlg()
{
}

MainDlg& MainDlg::Instance() 
{
    static MainDlg ms_Instanse;
    return ms_Instanse;
}

INT_PTR MainDlg::Run()
{
    Messages[WM_INITDIALOG] = &MainDlg::OnInit;
    Messages[WM_COMMAND]    = &MainDlg::OnCommand;
    Messages[WM_CLOSE]      = &MainDlg::OnClose;
    Messages[WM_DROPFILES]  = &MainDlg::OnDragDrop;

    Events[IDC_EXECUTE]     = &MainDlg::OnExecute;
    Events[IDC_NEW_PROC]    = &MainDlg::OnNewProcess;
    Events[IDC_LOAD_IMG]    = &MainDlg::OnLoadImage;
    Events[CBN_DROPDOWN]    = &MainDlg::OnDropDown;
    Events[CBN_SELCHANGE]   = &MainDlg::OnSelChange;

    return DialogBox( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDD_MAIN ), NULL, &MainDlg::DialogProcWrapper );
}

INT_PTR CALLBACK MainDlg::DialogProcWrapper(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (Messages.find( message ) != Messages.end())
        return (Instance().*Messages[message])( hDlg, message, wParam, lParam );

    return 0;
}

