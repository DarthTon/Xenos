#pragma once

#include "stdafx.h"
#include <string>

namespace dump
{
    /// <summary>
    /// Dump flags
    /// </summary>
    enum eDumpFlags
    {
        NoFlags = 0,
        CreateMinidump = 1,    // Create mini dump
        CreateFullDump = 2,    // Create full memory dump

        CreateBoth = CreateMinidump | CreateFullDump
    };

    /// <summary>
    /// Crash dump handler
    /// </summary>
    class DumpHandler
    {
    public:
        typedef int( *fnCallback )(const wchar_t*, void*, EXCEPTION_POINTERS*, bool);

    public:
        ~DumpHandler( void );

        /// <summary>
        /// Get singleton instance
        /// </summary>
        /// <returns>Instance</returns>
        static DumpHandler& Instance();

        /// <summary>
        /// Setup unhanded exception filter
        /// </summary>
        /// <param name="dumpRoot">Crash dump root dolder</param>
        /// <param name="flags">Dump flags</param>
        /// <param name="pDump">User crash callback routine</param>
        /// <param name="pFilter">Unused</param>
        /// <param name="pContext">User context to pass into crash callback</param>
        /// <returns>true on success, false if already installed</returns>
        bool CreateWatchdog(
            const std::wstring& dumpRoot,
            eDumpFlags flags,
            fnCallback pDump = nullptr,
            fnCallback pFilter = nullptr,
            void* pContext = nullptr
            );

        void DisableWatchdog();

    private:
        DumpHandler( void );

        /// <summary>
        /// Unhanded exception filter
        /// </summary>
        /// <param name="ExceptionInfo">Exception information</param>
        /// <returns>Exception handling status</returns>
        static LONG CALLBACK UnhandledExceptionFilter( PEXCEPTION_POINTERS ExceptionInfo );

        /// <summary>
        /// Write crash dump
        /// </summary>
        /// <param name="strFilePath">Output file</param>
        /// <param name="flags">flags</param>
        void CreateDump( const std::wstring& strFilePath, int flags );

        /// <summary>
        /// Crash watcher
        /// </summary>
        /// <param name="lpParam">DumpHandler pointer</param>
        /// <returns>Error code</returns>
        static DWORD CALLBACK WatchdogProc( LPVOID lpParam );

        /// <summary>
        /// Actual exception handler
        /// </summary>
        void ExceptionHandler();

        /// <summary>
        /// Generate crash dump file names
        /// </summary>
        /// <param name="strFullDump">Full dump name</param>
        /// <param name="strMiniDump">Mini dump name</param>
        /// <returns>true on success</returns>
        bool GenDumpFilenames( std::wstring& strFullDump, std::wstring& strMiniDump );

    private:
        void* _pPrevFilter = nullptr;                   // Previous unhanded exception filter
        bool _active = true;                            // Watchdog activity flag
        eDumpFlags _flags = NoFlags;                    // Dump creation flags
        DWORD _FailedThread = 0;                        // Crashed thread ID
        HANDLE _hWatchThd = NULL;                       // Watchdog thread handle
        PEXCEPTION_POINTERS _ExceptionInfo = nullptr;   // Crash exception info
        void* _pUserContext = nullptr;                  // User context for callback
        std::wstring _dumpRoot = L".";                  // Root folder for dump files
        fnCallback _pDumpProc = nullptr;                // Crash callback
    };
}

