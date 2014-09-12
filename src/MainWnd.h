#pragma once

#include "stdafx.h"
#include "resource.h"
#include "DlgModules.h"
#include "ConfigMgr.h"
#include "ComboBox.hpp"
#include "EditBox.hpp"
#include "CheckBox.hpp"
#include "Message.hpp"
#include "InjectionCore.h"

class MainDlg
{
    typedef INT_PTR( MainDlg::*fnDialogProc )(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    typedef std::map<UINT, fnDialogProc> mapMsgProc;
    typedef std::map<WORD, fnDialogProc> mapCtrlProc;

public:
    ~MainDlg();

    /// <summary>
    /// Get singleton instance
    /// </summary>
    /// <returns>Main dialog instance</returns>
    static MainDlg& Instance();

    /// <summary>
    /// Run GUI
    /// </summary>
    INT_PTR Run();

    /// <summary>
    /// Dialog message handler
    /// </summary>
    static INT_PTR CALLBACK DlgProcMain( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

private:
    MainDlg();
    MainDlg( MainDlg& root );
    MainDlg& operator=(MainDlg&);

private:
    /// <summary>
    /// Load configuration from file
    /// </summary>
    /// <returns>Error code</returns>
    DWORD LoadConfig();

    /// <summary>
    /// Save Configuration.
    /// </summary>
    /// <returns>Error code</returns>
    DWORD SaveConfig();

    /// <summary>
    /// Enumerate processes
    /// </summary>
    /// <returns>Error code</returns>
    DWORD FillProcessList();

    /// <summary>
    /// Retrieve process threads
    /// </summary>
    /// <param name="pid">Process ID</param>
    /// <returns>Error code</returns>
    DWORD FillThreads( DWORD pid );

    /// Set current process
    /// </summary>
    /// <param name="pid">Target PID</param>
    /// <param name="path">Process path.</param>
    /// <returns>Error code</returns>
    DWORD SetActiveProcess( DWORD pid, const std::wstring& name );

    /// <summary>
    /// Get manual map flags from interface
    /// </summary>
    /// <returns>Flags</returns>
    blackbone::eLoadFlags MmapFlags();

    /// <summary>
    /// Update interface manual map flags
    /// </summary>
    /// <param name="flags">Flags</param>
    /// <returns>Flags</returns>
    DWORD MmapFlags( blackbone::eLoadFlags flags );

    /// <summary>
    /// Set injection method
    /// </summary>
    /// <param name="mode">Injection mode</param>
    /// <returns>Error code</returns>
    DWORD SetMapMode( MapMode mode );

    //////////////////////////////////////////////////////////////////////////////////
    INT_PTR OnInit          ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam ); 
    INT_PTR OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnClose         ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    //////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////
    INT_PTR OnFileExit      ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnLoadImage     ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnExecute       ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnDropDown      ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnSelChange     ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnDragDrop      ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnNewProcess    ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnEjectModules  ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    INT_PTR OnProtectSelf   ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    //////////////////////////////////////////////////////////////////////////////////

private:
    HWND          _hMainDlg = NULL;
    mapMsgProc    _messages;
    mapCtrlProc   _events;
    InjectionCore _core;
    ConfigMgr     _config;
    std::wstring  _processPath;

    //
    // Interface controls
    //  
    ModulesDlg _modulesDlg;         // Module view

    ctrl::ComboBox _procList;       // Process list
    ctrl::ComboBox _threadList;     // Thread list
    ctrl::ComboBox _injectionType;  // Injection type
    ctrl::ComboBox _initFuncList;   // Module exports list

    ctrl::EditBox  _imagePath;      // Path to image
    ctrl::EditBox  _procCmdLine;    // Process arguments
    ctrl::EditBox  _initArg;        // Init routine argument

    ctrl::CheckBox _injClose;       // Close application after injection

    ctrl::CheckBox _unlink;         // Unlink image after injection   

    struct
    {
        ctrl::CheckBox addLdrRef;
        ctrl::CheckBox manualInmport;
        ctrl::CheckBox noTls;
        ctrl::CheckBox noExceptions;
        ctrl::CheckBox wipeHeader;
        ctrl::CheckBox hideVad;

    } _mmapOptions;
};