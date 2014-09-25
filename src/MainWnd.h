#pragma once

#include "resource.h"
#include "DlgModules.h"
#include "DlgWait.h"
#include "ConfigMgr.h"
#include "ComboBox.hpp"
#include "EditBox.hpp"
#include "Button.hpp"
#include "Message.hpp"
#include "InjectionCore.h"

class MainDlg : public Dialog
{

public:
    MainDlg();
    ~MainDlg();

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
    /// Update interface controls
    /// </summary>
    /// <param name="mode">Injection mode</param>
    /// <returns>Error code</returns>
    DWORD UpdateInterface( MapMode mode );

    /// <summary>
    /// Select executable image via file selection dialog
    /// </summary>
    /// <param name="selectedPath">Selected path</param>
    /// <returns>true if image was selected, false if canceled</returns>
    bool SelectExecutable( std::wstring& selectedPath );

    //
    // Message handlers
    //
    MSG_HANDLER( OnInit );
    MSG_HANDLER( OnClose );
    MSG_HANDLER( OnFileExit );
    MSG_HANDLER( OnLoadImage );
    MSG_HANDLER( OnSelectExecutable );
    MSG_HANDLER( OnExecute );
    MSG_HANDLER( OnDropDown );
    MSG_HANDLER( OnSelChange );
    MSG_HANDLER( OnDragDrop );
    MSG_HANDLER( OnExistingProcess );
    MSG_HANDLER( OnEjectModules );
    MSG_HANDLER( OnProtectSelf );

private:
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

    ctrl::Button _exProc;           // Existing process radio-button
    ctrl::Button _newProc;          // New process radio-button
    ctrl::Button _autoProc;         // Manual launch radio-button
    ctrl::Button _selectProc;       // Select process path
    ctrl::Button _injClose;         // Close application after injection
    ctrl::Button _unlink;           // Unlink image after injection   

    struct
    {
        ctrl::Button addLdrRef;
        ctrl::Button manualInmport;
        ctrl::Button noTls;
        ctrl::Button noExceptions;
        ctrl::Button wipeHeader;
        ctrl::Button hideVad;

    } _mmapOptions;
};