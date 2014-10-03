#pragma once

#include "window.hpp"
#include "../../BlackBone/src/BlackBone/Misc/Thunk.hpp"

/// <summary>
/// Generic dialog class
/// </summary>
class Dialog : public Window
{
public:
    typedef INT_PTR( Dialog::*fnDlgProc )(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    typedef std::map<UINT, fnDlgProc> mapMsgProc;
    typedef std::map<WORD, fnDlgProc> mapCtrlProc;

public:
    Dialog( int dialogID )
        : _dialogID( dialogID ) 
    { 
        _messages[WM_INITDIALOG] = &Dialog::OnInit;
        _messages[WM_COMMAND]    = &Dialog::OnCommand;
        _messages[WM_CLOSE]      = &Dialog::OnClose;
    }

    /// <summary>
    /// Run dialog as modal window
    /// </summary>
    /// <param name="parent">Parent window.</param>
    /// <returns>TRUE on success, FALSE otherwise</returns>
    virtual INT_PTR RunModal( HWND parent = NULL )
    {
        // Stupid std::function and std::bind don't support __stdcall
        // Had to do this idiotic workaround
        Win32Thunk<DLGPROC, Dialog> dlgProc( &Dialog::DlgProc, this );
        return DialogBoxW( NULL, MAKEINTRESOURCEW( _dialogID ), parent, dlgProc.GetThunk() );
    }

    /// <summary>
    /// Run dialog as modeless window
    /// </summary>
    /// <param name="parent">Parent window</param>
    /// <param name="accelID">Accelerator ID to use</param>
    /// <returns>TRUE on success, FALSE otherwise</returns>
    virtual INT_PTR RunModeless( HWND parent = NULL, int accelID = 0 )
    {
        MSG Msg = { 0 };
        _modeless = true;

        HACCEL hAccel = LoadAcceleratorsW( NULL, MAKEINTRESOURCEW( accelID ) );

        Win32Thunk<DLGPROC, Dialog> dlgProc( &Dialog::DlgProc, this );
        HWND hDlg = CreateDialogW( NULL, MAKEINTRESOURCE( _dialogID ), parent, dlgProc.GetThunk() );
        ShowWindow( hDlg, SW_SHOW );

        while (IsWindow( hDlg ) && GetMessage( &Msg, NULL, 0, 0 ))
        {
            if (TranslateAccelerator( hDlg, hAccel, &Msg ))
                continue;

            if (!IsDialogMessage( hDlg, &Msg ))
            {
                TranslateMessage( &Msg );
                DispatchMessage( &Msg );
            } 
        }

        return TRUE;
    }

protected:

    /// <summary>
    /// Close dialog properly
    /// </summary>
    /// <returns>status</returns>
    INT_PTR CloseDialog()
    {
        EndDialog( _hwnd, 0 );
        if (_modeless)
            DestroyWindow( _hwnd );

        return TRUE;
    }

    /// <summary>
    /// Dialog message proc
    /// </summary>
    INT_PTR DlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
    {
        if (_messages.count( message ))
            return (this->*_messages[message])(hDlg, message, wParam, lParam);

        return FALSE;
    }

    /// <summary>
    /// WM_INITDIALOG handler
    /// </summary>
    virtual INT_PTR OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
    {
        _hwnd = hDlg;
        return TRUE;
    }

    /// <summary>
    /// WM_COMMAND handler
    /// </summary>
    virtual INT_PTR OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
    {
        if (_events.count( LOWORD( wParam ) ))
            return (this->*_events[LOWORD( wParam )])(hDlg, message, wParam, lParam);

        if (_events.count( HIWORD( wParam ) ))
            return (this->*_events[HIWORD( wParam )])(hDlg, message, wParam, lParam);

        return FALSE;
    }

    /// <summary>
    /// WM_CLOSE handler
    /// </summary>
    virtual INT_PTR OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
    {
        return CloseDialog();
    }

protected:
    mapMsgProc _messages;   // Message handlers
    mapCtrlProc _events;    // Event handlers
    int _dialogID = 0;      // Dialog resource ID
    bool _modeless = false; // Modeless dialog
};

