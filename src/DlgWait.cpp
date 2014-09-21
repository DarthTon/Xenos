#include "DlgWait.h"

DlgWait* pInstance = nullptr;

DlgWait::DlgWait( InjectionCore& injection )
    : _injection( injection )
{
    pInstance = this;
}


DlgWait::~DlgWait()
{
}

INT_PTR CALLBACK DlgWait::DlgProcWait( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (pInstance->Messages.find( message ) != pInstance->Messages.end())
        return (pInstance->*pInstance->Messages[message])(hDlg, message, wParam, lParam);

    return 0;
}

INT_PTR DlgWait::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _hDlg = hDlg;
    return TRUE;
}

INT_PTR DlgWait::OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{

    if (Events.count( LOWORD( wParam ) ))
        return (this->*Events[LOWORD( wParam )])(hDlg, message, wParam, lParam);

    if (Events.count( HIWORD( wParam ) ))
        return (this->*Events[HIWORD( wParam )])(hDlg, message, wParam, lParam);

    return FALSE;
}

INT_PTR DlgWait::OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    EndDialog( hDlg, 0 );
    return TRUE;
}
