#pragma once

#include "stdafx.h"
#include "resource.h"
#include "ListView.hpp"

#include "../../BlackBone/src/BlackBone/Process/Process.h"
#include "../../BlackBone/src/BlackBone/Misc/Utils.h"

class ModulesDlg
{
    typedef INT_PTR( ModulesDlg::*PDLGPROC )(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    typedef std::map<UINT, PDLGPROC> mapMsgProc;
    typedef std::map<WORD, PDLGPROC> mapCtrlProc;

    enum ColumnID
    {
        Name = 0,
        ImageBase,
        Platform,
        LoadType
    };

public:
    ModulesDlg( blackbone::Process& proc );
    ~ModulesDlg();

    static INT_PTR CALLBACK DlgProcModules( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

private:
    void RefrestList();

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

    ctrl::ListView _modList;

    blackbone::Process& _process;
};