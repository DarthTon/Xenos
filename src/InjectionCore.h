#pragma once

#include "Message.hpp"
#include "ProfileMgr.h"
#include "Log.h"

#include <BlackBone/src/BlackBone/Config.h>
#include <BlackBone/src/BlackBone/Process/Process.h>
#include <BlackBone/src/BlackBone/PE/PEImage.h>
#include <BlackBone/src/BlackBone/Misc/Utils.h>

typedef std::vector<std::shared_ptr<blackbone::pe::PEImage>> vecPEImages;
typedef std::vector<blackbone::pe::vecExports> vecImageExports;

enum MapMode
{
    Normal = 0,         // Default - CreateRemoteThread/execute in existing thread
    Manual,             // Manual map
    Kernel_Thread,      // Kernel-mode CreateThread into LdrLoadDll
    Kernel_APC,         // Kernel-mode LdrLoadDll APC 
    Kernel_MMap,        // Kernel-mode manual map

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
    ProfileMgr::ConfigData cfg;                     // User config

    DWORD pid = 0;                                  // Target process ID
    vecPEImages images;                             // Images to inject
    std::wstring procPath;                          // Process path

    uint32_t skippedCount = 0;                      // Skipped processes    
    bool waitActive = false;                        // Process waiting state
    std::vector<blackbone::ProcessInfo> procList;   // Process list
    std::vector<blackbone::ProcessInfo> procDiff;   // Created processes list
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
    NTSTATUS GetTargetProcess( InjectContext& context, PROCESS_INFORMATION& pi );

    /// <summary>
    /// Inject multiple images
    /// </summary>
    /// <param name="pCtx">Injection context</param>
    /// <returns>Error code</returns>
    NTSTATUS InjectMultiple( InjectContext* pContext );

    inline blackbone::Process& process() { return _process; }

private:
    /// <summary>
    /// Validate initialization routine
    /// </summary>
    /// <param name="init">Routine name</param>
    /// <param name="initRVA">Routine RVA, if found</param>
    /// <returns>Error code</returns>
    NTSTATUS ValidateInit( const std::string& init, uint32_t& initRVA, blackbone::pe::PEImage& img );

    /// <summary>
    /// Validate all parameters
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    NTSTATUS ValidateContext( InjectContext& context, const blackbone::pe::PEImage& img );

    /// <summary>
    /// Injector thread worker
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    NTSTATUS InjectSingle( InjectContext& context, blackbone::pe::PEImage& img );

    /// <summary>
    /// Default injection method
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="pThread">Context thread of execution</param>
    /// <param name="mod">Resulting module</param>
    /// <returns>Error code</returns>
    blackbone::call_result_t<blackbone::ModuleDataPtr> InjectDefault(
        InjectContext& context,
        const blackbone::pe::PEImage& img,
        blackbone::ThreadPtr pThread = nullptr
        );

    /// <summary>
    /// Kernel-mode injection
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="img">Target image</param>
    /// <param name="initRVA">Init function RVA</param>
    /// <returns>Error code</returns>
    NTSTATUS InjectionCore::InjectKernel(
        InjectContext& context,
        const blackbone::pe::PEImage& img,
        uint32_t initRVA /*= 0*/
        );

    /// <summary>
    /// Manually map another system driver into system space
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <returns>Error code</returns>
    NTSTATUS MapDriver( InjectContext& context, const blackbone::pe::PEImage& img );

    /// <summary>
    /// Call initialization routine
    /// </summary>
    /// <param name="context">Injection context</param>
    /// <param name="mod">Target module</param>
    /// <param name="pThread">Context thread of execution</param>
    /// <returns>Error code</returns>
    NTSTATUS CallInitRoutine(
        InjectContext& context,
        const blackbone::pe::PEImage& img,
        blackbone::ModuleDataPtr mod,
        uint64_t exportRVA,
        blackbone::ThreadPtr pThread = nullptr
        );

private:
    HWND& _hMainDlg;                         // Owner dialog
    blackbone::Process _process;             // Target process
    std::vector<DWORD> _criticalProcList;    // List of processes with allocated physical pages
};

