#pragma once

#include "stdafx.h"
#include "resource.h"
#include "InjectionCore.h"

class DlgWait
{
    typedef INT_PTR( DlgWait::*PDLGPROC )(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    typedef std::map<UINT, PDLGPROC> mapMsgProc;
    typedef std::map<WORD, PDLGPROC> mapCtrlProc;

public:
    DlgWait( InjectionCore& injection );
    ~DlgWait();

    static INT_PTR CALLBACK DlgProcWait( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

    ////////////////////////////////////////////////////////////////////////////////
    INT_PTR OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );      //
    INT_PTR OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );   //
    INT_PTR OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );     //
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    INT_PTR OnCloseBtn( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );  //
    INT_PTR OnUnload( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    ////////////////////////////////////////////////////////////////////////////////

private:
    HWND        _hDlg = NULL;
    mapMsgProc  Messages;
    mapCtrlProc Events;

    InjectionCore& _injection;
};

