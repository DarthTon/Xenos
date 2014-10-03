#pragma once

#include "Message.hpp"

#include "../../BlackBone/src/BlackBone/Config.h"
#include "../../BlackBone/src/BlackBone/Process/Process.h"
#include "../../BlackBone/src/BlackBone/PE/PEImage.h"
#include "../../BlackBone/src/BlackBone/Misc/Utils.h"

typedef std::vector<blackbone::pe::PEImage> vecPEImages;
typedef std::vector<blackbone::pe::vecExports> vecImageExports;

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
    DWORD pid = 0;                                      // Target process ID
    vecPEImages images;                                 // Images to inject
    ProcMode procMode = Existing;                       // Process launch mode
    MapMode injectMode = Normal;                        // Injection type
    blackbone::eLoadFlags flags = blackbone::NoFlags;   // Manual map flags
    bool hijack = false;                                // Hijack existing thread
    bool unlinkImage = false;                           // Set to true to unlink image after injection
    bool erasePE = false;                               // Erase PE headers after native injection
    bool waitActive = false;                            // Process waiting state

    uint32_t delay = 0;                                 // Delay before injection
    uint32_t period = 0;                                // Period between images

    std::wstring procPath;                              // Process path
    std::wstring procCmdLine;                           // Process command line
    std::string  initRoutine;                           // Module initializer
    std::wstring initRoutineArg;                        // Module initializer params
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
    /// Inject multiple images
    /// </summary>
    /// <param name="pCtx">Injection context</param>
    /// <returns>Error code</returns>
    DWORD InjectMultiple( InjectContext* pContext );

    /// <summary>
    /// Waits for the injection thread to finish
    /// </summary>
    /// <returns>Injection status</returns>
    DWORD WaitOnInjection( InjectContext& pContext );

    inline blackbone::Process& process() { return _process; }

private:
    /// <summary>
    /// Validate initialization routine
    /// </summary>
    /// <param name="init">Routine name</param>
    /// <param name="initRVA">Routine RVA, if found</param>
    /// <returns>Error code</returns>
    DWORD ValidateInit( const std::string& init, uint32_t& initRVA, blackbone::pe::PEImage& img );

    /// <summary>
    /// Validate all parameters
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    DWORD ValidateContext( InjectContext& context, const blackbone::pe::PEImage& img );

    /// <summary>
    /// Injector thread worker
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    DWORD InjectSingle( InjectContext& context, blackbone::pe::PEImage& img );

    /// <summary>
    /// Default injection method
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="pThread">Context thread of execution</param>
    /// <param name="mod">Resulting module</param>
    /// <returns>Error code</returns>
    DWORD InjectDefault(
        InjectContext& context,
        const blackbone::pe::PEImage& img,
        blackbone::Thread* pThread,
        const blackbone::ModuleData* &mod
        );

    /// <summary>
    /// Kernel-mode injection
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="img">Target image</param>
    /// <param name="initRVA">Init function RVA</param>
    /// <returns>Error code</returns>
    DWORD InjectionCore::InjectKernel(
        InjectContext& context,
        const blackbone::pe::PEImage& img,
        uint32_t initRVA /*= 0*/
        );

    /// <summary>
    /// Manually map another system driver into system space
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    DWORD MapDriver( InjectContext& context, const blackbone::pe::PEImage& img );

    /// <summary>
    /// Call initialization routine
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="mod">Target module</param>
    /// <param name="pThread">Context thread of execution</param>
    /// <returns>Error code</returns>
    DWORD CallInitRoutine(
        InjectContext& context,
        const blackbone::pe::PEImage& img,
        const blackbone::ModuleData* mod,
        uint64_t exportRVA,
        blackbone::Thread* pThread
        );

private:
    HWND& _hMainDlg;             // Owner dialog
    blackbone::Process _process; // Target process
};

