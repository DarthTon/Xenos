#pragma once

#include "rapidxml_wrap.hpp"


class ProfileMgr
{
public:
    typedef std::vector<std::wstring> vecPaths;
    struct ConfigData
    {
        vecPaths images;                // Dll paths
        std::wstring procName;          // Target process name or full-qualified path
        std::wstring procCmdLine;       // Process command line
        std::wstring initRoutine;       // Dll initialization function
        std::wstring initArgs;          // Arguments passed into init function

        uint32_t pid = 0;               // Temporary pid for instant injection
        uint32_t mmapFlags = 0;         // Manual mapping flags
        uint32_t processMode = 0;       // Process launch mode
        uint32_t injectMode = 0;        // Injection type
        uint32_t delay = 0;             // Delay before injection
        uint32_t period = 0;            // Delay between images
        uint32_t skipProc = 0;          // Skip N first processes
        
        bool hijack = false;            // Hijack existing thread
        bool unlink = false;            // Unlink image after injection
        bool erasePE = false;           // Erase PE headers for native inject
        bool close = false;             // Close app after injection
        bool krnHandle = false;         // Escalate process handle access rights
        bool injIndef = false;          // Inject indefinitely
    };

public:
    bool Save( const std::wstring& path = L"" );
    bool Load( const std::wstring& path = L"" );

    inline ConfigData& config() { return _config; }

private:
    ConfigData _config;
};


