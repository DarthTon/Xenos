#pragma once

#include "Dialog.hpp"
#include "Button.hpp"
#include "ComboBox.hpp"
#include "EditBox.hpp"
#include "ProfileMgr.h"
#include "InjectionCore.h"


/// <summary>
/// Advanced settings dialog
/// </summary>
class DlgSettings : public Dialog
{
public:
    DlgSettings( ProfileMgr& cfgMgr );
    ~DlgSettings();

    inline void SetExports( const blackbone::pe::vecExports& exports ) { _exports = exports; }

private:
    /// <summary>
    /// Update interface controls
    /// </summary>
    void UpdateInterface();

    /// <summary>
    /// Handle BlackBone driver load problems
    /// </summary>
    /// <param name="type">Injection type</param>
    /// <returns>Error code</returns>
    DWORD HandleDriver( uint32_t type );

    /// <summary>
    /// Update interface accordingly to current config
    /// </summary>
    /// <returns>Error code</returns>
    DWORD UpdateFromConfig();

    /// <summary>
    /// Save Configuration.
    /// </summary>
    /// <returns>Error code</returns>
    DWORD SaveConfig();

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

    MSG_HANDLER( OnInit );
    MSG_HANDLER( OnOK );
    MSG_HANDLER( OnCancel );
    MSG_HANDLER( OnSelChange );
private:
    ProfileMgr& _profileMgr;                // Configuration manager
    blackbone::pe::vecExports _exports;     // Image exports
    uint32_t _lastSelected = Normal;        // Last selected injection mode

    //
    // Interface controls
    //
    ctrl::ComboBox _injectionType;  // Injection type
    ctrl::ComboBox _initFuncList;   // Module exports list

    ctrl::Button   _hijack;         // Thread hijack

    ctrl::EditBox  _procCmdLine;    // Process arguments
    ctrl::EditBox  _initArg;        // Init routine argument
    ctrl::EditBox  _delay;          // Delay before inject
    ctrl::EditBox  _period;         // Period between images
    ctrl::EditBox  _skipProc;       // Skip N first processes

    ctrl::Button _injClose;         // Close application after injection
    ctrl::Button _unlink;           // Unlink image after injection   
    ctrl::Button _erasePE;          // Erase PE headers after injection
    ctrl::Button _krnHandle;        // Escalate handle access rights
    ctrl::Button _injIndef;         // Inject indefinitely

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

