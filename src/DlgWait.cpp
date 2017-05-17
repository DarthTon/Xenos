#include "DlgWait.h"
#include <thread>

DlgWait::DlgWait( InjectionCore& core, InjectContext& context )
    : Dialog( IDD_WAIT_PROC )
    , _core( core )
    , _context( context )
    , _waitThread( &DlgWait::WaitForInjection, this )
{
    _events[ID_WAIT_CANCEL] = static_cast<Dialog::fnDlgProc>(&DlgWait::OnCloseBtn);
}

DlgWait::~DlgWait()
{
    if (_waitThread.joinable())
        _waitThread.join();
}

INT_PTR DlgWait::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    Dialog::OnInit( hDlg, message, wParam, lParam );
    std::wstring text = L"Awaiting '" + blackbone::Utils::StripPath( _context.procPath ) + L"' launch...";
    
    Static_SetText( GetDlgItem( hDlg, IDC_WAIT_TEXT ), text.c_str() );
    SendMessage( GetDlgItem( hDlg, IDC_WAIT_BAR ), PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)30 );

    return TRUE;
}

INT_PTR DlgWait::OnCloseBtn( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _context.waitActive = false;
    return TRUE;
}

/// <summary>
/// Wait for injection
/// </summary>
/// <returns>Error code</returns>
NTSTATUS DlgWait::WaitForInjection()
{
    for (bool inject = true; inject && _status != STATUS_REQUEST_ABORTED; inject = _context.cfg.injIndef)
        _status = _core.InjectMultiple( &_context );

    CloseDialog();
    return _status;
}