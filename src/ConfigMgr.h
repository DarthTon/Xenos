#pragma once

#include "rapidxml_wrap.hpp"


class ConfigMgr
{
public:
    struct ConfigData
    {
        std::wstring imagePath;         // Dll path
        std::wstring procName;          // Target process name or full-qualified path
        std::wstring procCmdLine;       // Process command line
        std::wstring initRoutine;       // Dll initialization function
        std::wstring initArgs;          // Arguments passed into init function

        uint32_t manualMapFlags = 0;    // Manual mapping flags
        uint32_t injectMode = 0;        // Injection type
        
        bool newProcess = false;        // Start new process instead of using existing
        bool threadHijack = false;      // Inject by hijacking existing process thread
        bool unlink = false;            // Unlink image after injection
        bool close = false;             // Close app after injection
    };

public:
    bool Save( const ConfigData& data );
    bool Load( ConfigData& data );
};


