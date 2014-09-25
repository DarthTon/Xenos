#pragma once

#include "Message.hpp"

#include "../../BlackBone/src/BlackBone/Config.h"
#include "../../BlackBone/src/BlackBone/Process/Process.h"
#include "../../BlackBone/src/BlackBone/PE/PEImage.h"
#include "../../BlackBone/src/BlackBone/Misc/Utils.h"


enum MapMode
{
    Normal = 0,         // Default - CreateRemoteThread/execute in existing thread
    Manual,             // Manual map
    Kernel_Thread,      // Kernel-mode CreateThread into LdrLoadDll
    Kernel_APC,         // Kernel-mode LdrLoadDll APC 

    Kernel_DriverMap,   // Kernel-mode driver mapping
};

enum ProcMode
{
    Existing = 0,       // Inject into existing process
    NewProcess,         // Create and inject
    ManualLaunch,       // Await process start and inject
};

/// <summary>
/// Injection params
/// </summary>
struct InjectContext
{
    DWORD pid = 0;                                          // Target process ID
    DWORD threadID = 0;                                     // Context thread ID
    ProcMode procMode = Existing;                           // Process launch mode
    MapMode injectMode = Normal;                            // Injection type
    blackbone::eLoadFlags flags = blackbone::NoFlags;       // Manual map flags
    bool unlinkImage = false;                               // Set to true to unlink image after injection

    std::wstring procPath;                                  // Process path
    std::wstring procCmdLine;                               // Process command line
    std::wstring imagePath;                                 // Image path
    std::string  initRoutine;                               // Module initializer
    std::wstring initRoutineArg;                            // Module initializer params
   
private:
    friend class InjectionCore;
    class InjectionCore* pCore = nullptr;
};

class InjectionCore
{
    typedef int( __stdcall* fnInitRoutine )(const wchar_t*);

public:
    InjectionCore( HWND& hMainDlg );
    ~InjectionCore();

    /// <summary>
    /// Get injection target
    /// </summary>
    /// <param name="context">Injection context.</param>
    /// <param name="pi">Process info in case of new process</param>
    /// <returns>Error code</returns>
    DWORD GetTargetProcess( InjectContext& context, PROCESS_INFORMATION& pi );

    /// <summary>
    /// Load selected image and do some validation
    /// </summary>
    /// <param name="path">Full qualified image path</param>
    /// <param name="exports">Image exports</param>
    /// <returns>Error code</returns>
    DWORD LoadImageFile( const std::wstring& path, blackbone::pe::listExports& exports );

    /// <summary>
    /// Initiate injection process
    /// </summary>
    /// <param name="ctx">Injection context</param>
    /// <returns>Error code</returns>
    DWORD DoInject( InjectContext& ctx );

    /// <summary>
    /// Waits for the injection thread to finish
    /// </summary>
    /// <returns>Injection status</returns>
    DWORD WaitOnInjection();

    /// <summary>
    /// Stops waiting for process
    /// </summary>
    inline void StopWait(){ _waitActive = false; }

    inline blackbone::Process& process() { return _proc; }
    inline InjectContext& lastContext() { return _context; }

private:
    /// <summary>
    /// Validate initialization routine
    /// </summary>
    /// <param name="init">Routine name</param>
    /// <param name="initRVA">Routine RVA, if found</param>
    /// <returns>Error code</returns>
    DWORD ValidateInit( const std::string& init, uint32_t& initRVA );

    /// <summary>
    /// Validate all parameters
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    DWORD ValidateContext( const InjectContext& context );

    /// <summary>
    /// Injector thread worker
    /// </summary>
    /// <param name="pCtx">Injection context</param>
    /// <returns>Error code</returns>
    DWORD InjectWorker( InjectContext* pCtx );

    /// <summary>
    /// Injector thread wrapper
    /// </summary>
    /// <param name="lpPram">Injection context</param>
    /// <returns>Error code</returns>
    static DWORD CALLBACK InjectWrapper( LPVOID lpParam );

    /// <summary>
    /// Kernel-mode injection
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="mod">Resulting module</param>
    /// <returns>Error code</returns>
    DWORD InjectKernel( InjectContext& context, const blackbone::ModuleData* &mod, uint32_t initRVA = 0 );

    /// <summary>
    /// Manually map another system driver into system space
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    DWORD MapDriver( InjectContext& context );

    /// <summary>
    /// Default injection method
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="pThread">Context thread of execution</param>
    /// <param name="mod">Resulting module</param>
    /// <returns>Error code</returns>
    DWORD InjectDefault(
        InjectContext& context,
        blackbone::Thread* pThread,
        const blackbone::ModuleData* &mod );

    /// <summary>
    /// Call initialization routine
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="mod">Target module</param>
    /// <param name="pThread">Context thread of execution</param>
    /// <returns>Error code</returns>
    DWORD CallInitRoutine(
        InjectContext& context,
        const blackbone::ModuleData* mod,
        uint64_t exportRVA,
        blackbone::Thread* pThread
        );

private:
    blackbone::Process     _proc;
    blackbone::pe::PEImage _imagePE;
    InjectContext          _context;
    HWND&                  _hMainDlg;
    HANDLE                 _lastThread = NULL;
    bool                   _waitActive = false;
};

