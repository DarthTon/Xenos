#include "stdafx.h"
#include "MainWnd.h"

MainDlg::MainDlg()
    : Dialog( IDD_MAIN )
    , _modulesDlg( _core.process() )
    , _core( _hwnd )
{
    _messages[WM_INITDIALOG] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnInit);
    _messages[WM_COMMAND]    = static_cast<Dialog::fnDlgProc>(&MainDlg::OnCommand);
    _messages[WM_CLOSE]      = static_cast<Dialog::fnDlgProc>(&MainDlg::OnClose);
    _messages[WM_DROPFILES]  = static_cast<Dialog::fnDlgProc>(&MainDlg::OnDragDrop);

    _events[IDC_EXECUTE]            = static_cast<Dialog::fnDlgProc>(&MainDlg::OnExecute);
    _events[IDC_NEW_PROC]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelectExecutable);
    _events[IDC_EXISTING_PROC]      = static_cast<Dialog::fnDlgProc>(&MainDlg::OnExistingProcess);
    _events[IDC_AUTO_PROC]          = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelectExecutable);
    _events[IDC_SELECT_PROC]        = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelectExecutable);
    _events[IDC_LOAD_IMG]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnLoadImage);
    _events[CBN_DROPDOWN]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnDropDown);
    _events[CBN_SELCHANGE]          = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelChange);
    _events[ID_TOOLS_PROTECT]       = static_cast<Dialog::fnDlgProc>(&MainDlg::OnProtectSelf);
    _events[ID_TOOLS_EJECTMODULES]  = static_cast<Dialog::fnDlgProc>(&MainDlg::OnEjectModules);
}

MainDlg::~MainDlg()
{
}
