#include "InjectionCore.h"
#include <BlackBone/src/BlackBone/Process/RPC/RemoteFunction.hpp>
#include <iterator>

InjectionCore::InjectionCore( HWND& hMainDlg )
    : _hMainDlg( hMainDlg )
{
    // Load driver
    //blackbone::Driver().EnsureLoaded();
}


InjectionCore::~InjectionCore()
{
    //
    // If at least one process with allocated physical pages exist, prevent driver unload or process will crash
    // Although PID can be reused, I'm too lazy to implement more reliable detection
    //
    std::vector<DWORD> existing = blackbone::Process::EnumByName( L"" ), mutual;
    std::sort( existing.begin(), existing.end() );
    std::sort( _criticalProcList.begin(), _criticalProcList.end() );
    std::set_intersection( existing.begin(), existing.end(), _criticalProcList.begin(), _criticalProcList.end(), std::back_inserter( mutual ) );
    if (mutual.empty())
        blackbone::Driver().Unload();
}


/// <summary>
/// Get injection target
/// </summary>
/// <param name="context">Injection context.</param>
/// <param name="pi">Process info in case of new process</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::GetTargetProcess( InjectContext& context, PROCESS_INFORMATION& pi )
{
    NTSTATUS status = ERROR_SUCCESS;

    // No process required for driver mapping
    if (context.cfg.injectMode == Kernel_DriverMap)
        return status;

    // Await new process
    if (context.cfg.processMode == ManualLaunch)
    {
        xlog::Normal( "Waiting on process %ls", context.procPath.c_str() );

        auto procName = blackbone::Utils::StripPath( context.procPath );
        if (procName.empty())
        {
            Message::ShowWarning( _hMainDlg, L"Please select executable to wait for\n" );
            return STATUS_NOT_FOUND;
        }
        
        // Filter already existing processes
        std::vector<blackbone::ProcessInfo> newList;

        if (context.procList.empty())
            context.procList = blackbone::Process::EnumByNameOrPID( 0, procName ).result( std::vector<blackbone::ProcessInfo>() );

        // Wait for process
        for (context.waitActive = true;; Sleep( 10 ))
        {
            // Canceled by user
            if (!context.waitActive)
            {
                xlog::Warning( "Process wait canceled by user" );
                return STATUS_REQUEST_ABORTED;
            }

            if (!context.procDiff.empty())
            {
                context.procList = newList;

                // Skip first N found processes
                if (context.skippedCount < context.cfg.skipProc)
                {
                    context.skippedCount++;
                    context.procDiff.erase( context.procDiff.begin() );

                    continue;
                }

                context.pid = context.procDiff.front().pid;
                context.procDiff.erase( context.procDiff.begin() );

                xlog::Verbose( "Got process %d", context.pid );
                break;
            }
            else
            {  
                // Detect new process
                newList = blackbone::Process::EnumByNameOrPID( 0, procName ).result( std::vector<blackbone::ProcessInfo>() );
                std::set_difference( 
                    newList.begin(), newList.end(), 
                    context.procList.begin(), context.procList.end(), 
                    std::inserter( context.procDiff, context.procDiff.begin() ) 
                    );
            }
        }
    }
    // Create new process
    else if (context.cfg.processMode == NewProcess)
    {
        STARTUPINFOW si = { 0 };
        si.cb = sizeof( si );

        xlog::Normal( "Creating new process '%ls' with arguments '%ls'", context.procPath.c_str(), context.cfg.procCmdLine.c_str() );

        if (!CreateProcessW( context.procPath.c_str(), (LPWSTR)context.cfg.procCmdLine.c_str(),
            NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, blackbone::Utils::GetParent( context.procPath ).c_str(), &si, &pi ))
        {
            Message::ShowError( _hMainDlg, L"Failed to create new process.\n" + blackbone::Utils::GetErrorDescription( LastNtStatus() ) );
            return LastNtStatus();
        }

        // Escalate handle access rights through driver
        if (context.cfg.krnHandle)
        {
            xlog::Normal( "Escalating process handle access" );

            status = _process.Attach( pi.dwProcessId, PROCESS_QUERY_LIMITED_INFORMATION );
            if (NT_SUCCESS( status ))
            {
                status = blackbone::Driver().PromoteHandle(
                    GetCurrentProcessId(),
                    _process.core().handle(),
                    DEFAULT_ACCESS_P | PROCESS_QUERY_LIMITED_INFORMATION
                    );
            }
        }
        else
        {
            // Attach for thread init
            status = _process.Attach( pi.dwProcessId );
        }

        // Create new thread to make sure LdrInitializeProcess gets called
        if (NT_SUCCESS( status ))
            _process.EnsureInit();

        if (!NT_SUCCESS( status ))
        {
            xlog::Error( "Failed to attach to process, status 0x%X", status );
            return status;
        }

        // No process handle is required for kernel injection
        if (context.cfg.injectMode >= Kernel_Thread)
        {
            context.pid = pi.dwProcessId;
            _process.Detach();

            // Thread must be running for APC execution
            if (context.cfg.injectMode == Kernel_APC)
            {
                ResumeThread( pi.hThread );
                Sleep( 100 );
            }
        }

        // Because the only suitable thread is suspended, thread hijacking can't be used
        context.cfg.hijack = false;
    }
    // Attach to existing process
    if (context.cfg.injectMode < Kernel_Thread && context.cfg.processMode != NewProcess)
    {
        // Escalate handle access rights through driver
        if (context.cfg.krnHandle)
        {
            xlog::Normal( "Escalating process handle access" );

            status = _process.Attach( context.pid, PROCESS_QUERY_LIMITED_INFORMATION );
            if (NT_SUCCESS( status ))
            {
                status = blackbone::Driver().PromoteHandle(
                    GetCurrentProcessId(),
                    _process.core().handle(),
                    DEFAULT_ACCESS_P | PROCESS_QUERY_LIMITED_INFORMATION
                    );
            }

            if (!NT_SUCCESS( status ))
                xlog::Error( "Failed to escalate process handle access, status 0x%X", status );
        }
        else
            status = _process.Attach( context.pid );

        if (NT_SUCCESS( status ) && !_process.valid())
            status = STATUS_PROCESS_IS_TERMINATING;

        if (status != STATUS_SUCCESS)
        {
            std::wstring errmsg = L"Can not attach to the process.\n" + blackbone::Utils::GetErrorDescription( status );
            Message::ShowError( _hMainDlg, errmsg );

            return status;
        }


        //
        // Make sure loader is initialized
        //
        bool ldrInit = false;
        if (_process.core().isWow64())
        {
            blackbone::_PEB32 peb32 = { 0 };
            _process.core().peb32( &peb32 );
            ldrInit = peb32.Ldr != 0;
        }
        else
        {
            blackbone::_PEB64 peb64 = { 0 };
            _process.core().peb64( &peb64 );
            ldrInit = peb64.Ldr != 0;
        }

        // Create new thread to make sure LdrInitializeProcess gets called
        if (!ldrInit)
            _process.EnsureInit();
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Validate all parameters
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::ValidateContext( InjectContext& context, const blackbone::pe::PEImage& img )
{
    // Invalid path
    if (context.images.empty())
    {
        Message::ShowError( _hMainDlg, L"Please add at least one image to inject" );
        return STATUS_INVALID_PARAMETER_1;
    }

    // Validate driver
    if (context.cfg.injectMode == Kernel_DriverMap)
    {
        // Only x64 drivers are supported
        if (img.mType() != blackbone::mt_mod64)
        {
            Message::ShowError( _hMainDlg, L"Can't map x86 drivers - '" + img.name() + L"'" );
            return STATUS_INVALID_IMAGE_WIN_32;
        }

        // Image must be native
        if (img.subsystem() != IMAGE_SUBSYSTEM_NATIVE)
        {
            Message::ShowError( _hMainDlg, L"Can't map image with non-native subsystem - '" + img.name() + L"'" );
            return STATUS_INVALID_IMAGE_HASH;
        }

        return STATUS_SUCCESS;
    }

    // No process selected
    if (!_process.valid())
    {
        Message::ShowError( _hMainDlg, L"Please select valid process before injection" );
        return STATUS_INVALID_HANDLE;
    }

    auto& barrier = _process.barrier();

    // Validate architecture
    if (!img.pureIL() && img.mType() == blackbone::mt_mod32 && barrier.targetWow64 == false)
    {
        Message::ShowError( _hMainDlg, L"Can't inject 32 bit image '" + img.name() + L"' into native 64 bit process" );
        return STATUS_INVALID_IMAGE_WIN_32;
    }

    // Trying to inject x64 dll into WOW64 process
    if (!img.pureIL() && img.mType() == blackbone::mt_mod64 && barrier.targetWow64 == true)
    {
        Message::ShowError( _hMainDlg, L"Can't inject 64 bit image '" + img.name() + L"' into WOW64 process" );
        return STATUS_INVALID_IMAGE_WIN_64;
    }

    // Can't inject managed dll through WOW64 barrier
    /*if (img.pureIL() && (barrier.type == blackbone::wow_32_64 || barrier.type == blackbone::wow_64_32))
    {
        if (barrier.type == blackbone::wow_32_64)
            Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to inject managed dll '" + img.name() + L"' into x64 process" );
        else
            Message::ShowWarning( _hMainDlg, L"Please use Xenos.exe to inject managed dll '" + img.name() + L"' into WOW64 process" );

        return STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
    }*/

    // Can't inject 64 bit image into WOW64 process from x86 version
    /*if (img.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_32_32)
    {
        Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to inject 64 bit image '" + img.name() + L"' into WOW64 process" );
        return STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
    }*/

    // Can't execute code in another thread trough WOW64 barrier
    /*if (context.cfg.hijack && barrier.type != blackbone::wow_32_32 &&  barrier.type != blackbone::wow_64_64)
    {
        Message::ShowError( _hMainDlg, L"Can't execute code in existing thread trough WOW64 barrier" );
        return STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
    }*/

    // Manual map restrictions
    if (context.cfg.injectMode == Manual || context.cfg.injectMode == Kernel_MMap)
    {
        if (img.pureIL() && (context.cfg.injectMode == Kernel_MMap || !img.isExe()))
        {
            Message::ShowError( _hMainDlg, L"Pure managed class library '" + img.name() + L"' can't be manually mapped yet" );
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        /*if (((img.mType() == blackbone::mt_mod32 && barrier.sourceWow64 == false) ||
            (img.mType() == blackbone::mt_mod64 && barrier.sourceWow64 == true)))
        {
            if (img.mType() == blackbone::mt_mod32)
                Message::ShowWarning( _hMainDlg, L"Please use Xenos.exe to manually map 32 bit image '" + img.name() + L"'" );
            else
                Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to manually map 64 bit image '" + img.name() + L"'" );

            return STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
        }*/
    }

    return STATUS_SUCCESS;
}

/// <summary>
/// Validate initialization routine
/// </summary>
/// <param name="init">Routine name</param>
/// <param name="initRVA">Routine RVA, if found</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::ValidateInit( const std::string& init, uint32_t& initRVA, blackbone::pe::PEImage& img )
{
    // Validate init routine
    if (img.pureIL())
    {
        // Pure IL exe doesn't need init routine
        if (img.isExe())
        {
            initRVA = 0;
            return STATUS_SUCCESS;
        };

        blackbone::ImageNET::mapMethodRVA methods;
        img.net().Parse( &methods );
        bool found = false;

        if (!methods.empty() && !init.empty())
        {
            auto winit = blackbone::Utils::AnsiToWstring( init );

            for (auto& val : methods)
                if (winit == (val.first.first + L"." + val.first.second))
                {
                    found = true;
                    break;
                }
        }

        if (!found)
        {
            if (init.empty())
            {
                Message::ShowError( _hMainDlg, L"Please select '" + img.name() + L"' entry point" );
            }
            else
            {
                auto str = blackbone::Utils::FormatString(
                    L"Image '%ls' does not contain specified method - '%ls'",
                    img.name().c_str(),
                    blackbone::Utils::AnsiToWstring( init.c_str() ).c_str()
                    );

                Message::ShowError( _hMainDlg, str );
            }

            return STATUS_NOT_FOUND;
        }
    }
    else if (!init.empty())
    {
        blackbone::pe::vecExports exports;
        img.GetExports( exports );

        auto iter = std::find_if(
            exports.begin(), exports.end(),
            [&init]( const blackbone::pe::ExportData& val ){ return val.name == init; } );

        if (iter == exports.end())
        {
            initRVA = 0;
            auto str = blackbone::Utils::FormatString(
                L"Image '%ls' does not contain specified export - '%ls'",
                img.name().c_str(),
                blackbone::Utils::AnsiToWstring( init.c_str() ).c_str()
                );

            Message::ShowError( _hMainDlg, str );
            return STATUS_NOT_FOUND;
        }
        else
        {
            initRVA = iter->RVA;
        }
    }

    return STATUS_SUCCESS;
}


/// <summary>
/// Inject multiple images
/// </summary>
/// <param name="pCtx">Injection context</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::InjectMultiple( InjectContext* pContext )
{
    NTSTATUS status = ERROR_SUCCESS;
    PROCESS_INFORMATION pi = { 0 };

    // Update PID
    if(pContext->cfg.injectMode == Existing && pContext->pid == 0)
        pContext->pid = pContext->cfg.pid;

    // Log some info
    xlog::Critical(
        "Injection initiated. Mode: %d, process type: %d, pid: %d, mmap flags: 0x%X, "
        "erasePE: %d, unlink: %d, thread hijack: %d, init routine: '%s', init arg: '%ls'",
        pContext->cfg.injectMode,
        pContext->cfg.processMode,
        pContext->pid,
        pContext->cfg.mmapFlags,
        pContext->cfg.erasePE,
        pContext->cfg.unlink,
        pContext->cfg.hijack,
        pContext->cfg.initRoutine.c_str(),
        pContext->cfg.initArgs.c_str()
        );

    // Get process for injection
    status = GetTargetProcess( *pContext, pi );
    if (status != STATUS_SUCCESS)
        return status;

    if (pContext->cfg.delay)
        Sleep( pContext->cfg.delay );

    // Inject all images
    for (auto& img : pContext->images)
    {
        status |= InjectSingle( *pContext, *img );
        if (pContext->cfg.period)
            Sleep( pContext->cfg.period );
    }

    //
    // Cleanup after injection
    //
    if (pi.hThread)
    {
        if (status == STATUS_SUCCESS)
            ResumeThread( pi.hThread );

        CloseHandle( pi.hThread );
    }

    if (pi.hProcess)
    {
        // TODO: Decide if process should be terminated if at least one module failed to inject
        /*if (errCode != ERROR_SUCCESS)
            TerminateProcess( pi.hProcess, 0 );*/

        CloseHandle( pi.hProcess );
    }

    // Save PID if using physical memory allocation
    if (status == STATUS_SUCCESS && (pContext->cfg.mmapFlags & blackbone::HideVAD) &&
         (pContext->cfg.injectMode == Manual || pContext->cfg.injectMode == Kernel_MMap))
    {
        _criticalProcList.emplace_back( _process.pid() );
    }

    if (_process.core().handle())
        _process.Detach();

    return status;
}


/// <summary>
/// Injector thread worker
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::InjectSingle( InjectContext& context, blackbone::pe::PEImage& img )
{
    NTSTATUS status = ERROR_SUCCESS;
    blackbone::ThreadPtr pThread;
    blackbone::ModuleDataPtr mod;
    uint32_t exportRVA = 0;

    xlog::Critical( "Injecting image '%ls'", img.path().c_str() );

    // Check export
    status = ValidateInit( blackbone::Utils::WstringToUTF8( context.cfg.initRoutine ), exportRVA, img );
    if (!NT_SUCCESS( status ))
    {
        xlog::Error( "Image init routine check failed, status: 0x%X", status );
        return status;
    }

    // Final sanity check
    if (context.cfg.injectMode < Kernel_Thread || context.cfg.injectMode == Kernel_DriverMap)
    {
        if (!NT_SUCCESS( status = ValidateContext( context, img ) ))
        {
            xlog::Error( "Context validation failed, status: 0x%X", status );
            return status;
        }
    }

    // Get context thread
    if (context.cfg.hijack && context.cfg.injectMode < Kernel_Thread)
    {
        xlog::Normal( "Searching for thread to hijack" );
        pThread = _process.threads().getMostExecuted();
        if (!pThread)
        {
            Message::ShowError( _hMainDlg, L"Failed to get suitable thread for execution");
            return status = STATUS_NOT_FOUND;
        }
    }

    auto modCallback = []( blackbone::CallbackType type, void*, blackbone::Process&, const blackbone::ModuleData& modInfo )
    {
        if (type == blackbone::PreCallback)
        {
            if (modInfo.name == L"user32.dll")
                return blackbone::LoadData( blackbone::MT_Native, blackbone::Ldr_Ignore );
        }

        return blackbone::LoadData( blackbone::MT_Default, blackbone::Ldr_Ignore );
    };

    // Actual injection
    if (NT_SUCCESS( status ))
    {
        switch (context.cfg.injectMode)
        {
            case Normal:
                {
                    auto injectedMod = InjectDefault( context, img, pThread );
                    if (!injectedMod)
                        status = injectedMod.status;
                    else
                        mod = injectedMod.result();
                }
                break;

            case Manual:
                {
                    auto flags = static_cast<blackbone::eLoadFlags>(context.cfg.mmapFlags);
                    if (img.isExe())
                        flags |= blackbone::RebaseProcess;

                    auto injectedMod = _process.mmap().MapImage( img.path(), flags, modCallback );
                    if (!injectedMod)
                    {
                        status = injectedMod.status;
                        xlog::Error( "Failed to inject image using manual map, status: 0x%X", injectedMod.status );
                    }
                    else
                        mod = injectedMod.result();
                }
                break;

            case Kernel_Thread:
            case Kernel_APC:
            case Kernel_MMap:
                if (!NT_SUCCESS( status = InjectKernel( context, img, exportRVA ) ))
                    xlog::Error( "Failed to inject image using kernel injection, status: 0x%X", status );
                break;                

            case Kernel_DriverMap:
                status = MapDriver( context, img );
                break;

            default:
                break;
        }
    }

    // Fix error code
    if (!img.pureIL() && mod == nullptr && context.cfg.injectMode < Kernel_Thread && NT_SUCCESS( status ))
    {
        xlog::Error( "Invalid failure status: 0x%X", status );
        status = STATUS_UNSUCCESSFUL;
    }

    // Initialize routine
    if (NT_SUCCESS( status ) && context.cfg.injectMode < Kernel_Thread)
    {
        status = CallInitRoutine( context, img, mod, exportRVA, pThread );
    }
    else if (!NT_SUCCESS( status ))
    {
        wchar_t errBuf[128] = { 0 };
        wsprintfW( errBuf, L"Failed to inject image '%ls'.\nError code 0x%X", img.path().c_str(), status );
        Message::ShowError( _hMainDlg, errBuf );
    }

    // Erase header
    if (NT_SUCCESS( status ) && mod && context.cfg.injectMode == Normal && context.cfg.erasePE)
    {
        auto size = img.headersSize();
        DWORD oldProt = 0;
        std::unique_ptr<uint8_t[]> zeroBuf( new uint8_t[size]() );

        _process.memory().Protect( mod->baseAddress, size, PAGE_EXECUTE_READWRITE, &oldProt );
        _process.memory().Write( mod->baseAddress, size, zeroBuf.get() );
        _process.memory().Protect( mod->baseAddress, size, oldProt );
    }

    // Unlink module if required
    if (NT_SUCCESS( status ) && mod && context.cfg.injectMode == Normal && context.cfg.unlink)
        if (_process.modules().Unlink( mod ) == false)
        {
            status = ERROR_FUNCTION_FAILED;
            Message::ShowError( _hMainDlg, L"Failed to unlink module '" + img.path() + L"'" );
        }
    
    return status;
}


/// <summary>
/// Default injection method
/// </summary>
/// <param name="context">Injection context</param>
/// <param name="pThread">Context thread of execution</param>
/// <param name="mod">Resulting module</param>
/// <returns>Error code</returns>
blackbone::call_result_t<blackbone::ModuleDataPtr> InjectionCore::InjectDefault(
    InjectContext& context, 
    const blackbone::pe::PEImage& img,
    blackbone::ThreadPtr pThread /*= nullptr*/
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    // Pure IL image
    if (img.pureIL())
    {
        DWORD code = 0;

        xlog::Normal( "Image '%ls' is pure IL", img.path().c_str() );

        if (!_process.modules().InjectPureIL(
            blackbone::ImageNET::GetImageRuntimeVer( img.path().c_str() ),
            img.path(),
            context.cfg.initRoutine,
            context.cfg.initArgs,
            code ))
        {
            if (code == ERROR_SUCCESS)
                code = STATUS_UNSUCCESSFUL;

            xlog::Error( "Failed to inject pure IL image, status: %d", code );
            if (NT_SUCCESS( code ))
                code = STATUS_UNSUCCESSFUL;

            return code;
        }

        auto mod = _process.modules().GetModule( img.name(), blackbone::Sections );
        return mod ? blackbone::call_result_t<blackbone::ModuleDataPtr>( mod ) 
                   : blackbone::call_result_t<blackbone::ModuleDataPtr>( STATUS_NOT_FOUND );
    }
    else
    {
        auto injectedMod = _process.modules().Inject( img.path(), pThread );
        if (!injectedMod)
            xlog::Error( "Failed to inject image using default injection, status: 0x%X", injectedMod.status );

        return injectedMod;
    }
}

/// <summary>
/// Kernel-mode injection
/// </summary>
/// <param name="context">Injection context</param>
/// <param name="img">Target image</param>
/// <param name="initRVA">Init function RVA</param>
NTSTATUS InjectionCore::InjectKernel(
    InjectContext& context,
    const blackbone::pe::PEImage& img,
    uint32_t initRVA /*= 0*/
    )
{
    if (context.cfg.injectMode == Kernel_MMap)
    {
        return blackbone::Driver().MmapDll(
            context.pid,
            img.path(),
            (KMmapFlags)context.cfg.mmapFlags,
            initRVA,
            context.cfg.initArgs
            );
    }
    else
    {
        return blackbone::Driver().InjectDll(
            context.pid,
            img.path(),
            (context.cfg.injectMode == Kernel_Thread ? IT_Thread : IT_Apc),
            initRVA,
            context.cfg.initArgs,
            context.cfg.unlink,
            context.cfg.erasePE
            );
    }
}

/// <summary>
/// Manually map another system driver into system space
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::MapDriver( InjectContext& context, const blackbone::pe::PEImage& img )
{
    return blackbone::Driver().MMapDriver( img.path() );
}

/// <summary>
/// Call initialization routine
/// </summary>
/// <param name="context">Injection context</param>
/// <param name="mod">Target module</param>
/// <param name="pThread">Context thread of execution</param>
/// <returns>Error code</returns>
NTSTATUS InjectionCore::CallInitRoutine(
    InjectContext& context,
    const blackbone::pe::PEImage& img,
    blackbone::ModuleDataPtr mod,
    uint64_t exportRVA,
    blackbone::ThreadPtr pThread /*= nullptr*/
    )
{
    // Call init for native image
    if (!context.cfg.initRoutine.empty() && !img.pureIL() && context.cfg.injectMode < Kernel_Thread)
    {
        auto fnPtr = mod->baseAddress + exportRVA;

        xlog::Normal( "Calling initialization routine for image '%ls'", img.path().c_str() );

        // Create new thread
        if (pThread == nullptr)
        {
            auto argMem = _process.memory().Allocate( 0x1000, PAGE_READWRITE );
            if (!argMem)
                return argMem.status;

            argMem->Write( 0, context.cfg.initArgs.length() * sizeof( wchar_t ) + 2, context.cfg.initArgs.c_str() );
            auto status = _process.remote().ExecDirect( fnPtr, argMem->ptr() );

            xlog::Normal( "Initialization routine returned 0x%X", status );
        }
        // Execute in existing thread
        else
        {
            blackbone::RemoteFunction<fnInitRoutine> pfn( _process, fnPtr );
            auto status = pfn.Call( context.cfg.initArgs.c_str(), pThread );

            xlog::Normal( "Initialization routine returned 0x%X", status );
        }
    }

    return STATUS_SUCCESS;
}

