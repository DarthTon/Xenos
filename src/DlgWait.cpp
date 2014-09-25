#include "DlgWait.h"
#include <thread>

DlgWait::DlgWait( InjectionCore& injection )
    : Dialog( IDD_WAIT_PROC )
    , _injection( injection )
{
    _events[ID_WAIT_CANCEL] = static_cast<Dialog::fnDlgProc>(&DlgWait::OnCloseBtn);
}

DlgWait::~DlgWait()
{
}

INT_PTR DlgWait::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    Dialog::OnInit( hDlg, message, wParam, lParam );
    auto& context = _injection.lastContext();
    std::wstring text = L"Awaiting '" + blackbone::Utils::StripPath( context.procPath ) + L"' launch...";
    
    Static_SetText( GetDlgItem( hDlg, IDC_WAIT_TEXT ), text.c_str() );
    SendMessage( GetDlgItem( hDlg, IDC_WAIT_BAR ), PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)30 );

    // Wait for injection
    std::thread( &DlgWait::WaitForInjection, this ).detach();

    return TRUE;
}

INT_PTR DlgWait::OnCloseBtn( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _injection.StopWait();
    return Dialog::OnClose( hDlg, message, wParam, lParam );
}

/// <summary>
/// Wait for injection
/// </summary>
/// <returns>Error code</returns>
DWORD DlgWait::WaitForInjection()
{
    DWORD code = _injection.WaitOnInjection();

    EndDialog( _hwnd, 0 );
    return code;
}