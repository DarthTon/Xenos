#include "InjectionCore.h"
#include "../../BlackBone/src/BlackBone/Process/RPC/RemoteFunction.hpp"
#include <iterator>

InjectionCore::InjectionCore( HWND& hMainDlg )
    : _hMainDlg( hMainDlg )
{
    // Load driver
    blackbone::Driver().EnsureLoaded();
}


InjectionCore::~InjectionCore()
{
    //
    // If at least one process with allocated physical pages exist, prevent driver unload or process will crash
    // Although PID can be reused, I'm too lazy to implement more reliable detection
    //
    std::vector<DWORD> existing, mutual;
    blackbone::Process::EnumByName( L"", existing );
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
DWORD InjectionCore::GetTargetProcess( InjectContext& context, PROCESS_INFORMATION& pi )
{
    NTSTATUS status = ERROR_SUCCESS;

    // No process required for driver mapping
    if (context.injectMode == Kernel_DriverMap)
        return status;

    // Await new process
    if (context.procMode == ManualLaunch)
    {
        xlog::Normal( "Waiting on process %ls", context.procPath.c_str() );

        auto procName = blackbone::Utils::StripPath( context.procPath );
        if (procName.empty())
        {
            Message::ShowWarning( _hMainDlg, L"Please select executable to wait for\n" );
            return STATUS_NOT_FOUND;
        }
        
        // Filter already existing processes
        std::vector<blackbone::ProcessInfo> existing, newList, diff;
        blackbone::Process::EnumByNameOrPID( 0, procName, existing );

        // Wait for process
        for (context.waitActive = true;; Sleep( 10 ))
        {
            // Canceled by user
            if (!context.waitActive)
            {
                xlog::Warning( "Process wait canceled by user" );
                return ERROR_CANCELLED;
            }

            // Detect new process
            diff.clear();
            blackbone::Process::EnumByNameOrPID( 0, procName, newList );
            std::set_difference( newList.begin(), newList.end(), existing.begin(), existing.end(), std::inserter( diff, diff.begin() ) );

            if (!diff.empty())
            {
                context.pid = diff.front().pid;
                xlog::Verbose( "Got process %d", context.pid );
                break;
            }
        }
    }
    // Create new process
    else if (context.procMode == NewProcess)
    {
        STARTUPINFOW si = { 0 };
        si.cb = sizeof( si );

        xlog::Normal( "Creating new process '%ls' with arguments '%ls'", context.procPath.c_str(), context.procCmdLine.c_str() );

        if (!CreateProcessW( context.procPath.c_str(), (LPWSTR)context.procCmdLine.c_str(),
            NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
        {
            Message::ShowError( _hMainDlg, L"Failed to create new process.\n" + blackbone::Utils::GetErrorDescription( LastNtStatus() ) );
            return GetLastError();
        }

        // Escalate handle access rights through driver
        if (context.krnHandle)
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

            if (!NT_SUCCESS( status ))
                xlog::Error( "Failed to escalate process handle access, status 0x%X", status );
        }
        else
        {
            status = _process.Attach( pi.dwProcessId, PROCESS_ALL_ACCESS );
            if (!NT_SUCCESS( status ))
                xlog::Error( "Failed to attach to process, status 0x%X", status );
        }

        // Create new thread to make sure LdrInitializeProcess gets called
        if (NT_SUCCESS( status ))
            _process.EnsureInit();

        // No process handle is required for kernel injection
        if (context.injectMode >= Kernel_Thread)
        {
            context.pid = pi.dwProcessId;
            _process.Detach();

            // Thread must be running for APC execution
            if (context.injectMode == Kernel_APC)
            {
                ResumeThread( pi.hThread );
                Sleep( 100 );
            }
        }

        // Because the only suitable thread is suspended, thread hijacking can't be used
        context.hijack = false;
    }
    // Attach to existing process
    if (context.injectMode < Kernel_Thread && context.procMode != NewProcess)
    {
        // Escalate handle access rights through driver
        if (context.krnHandle)
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
            _process.core().peb( &peb32 );
            ldrInit = peb32.Ldr != 0;
        }
        else
        {
            blackbone::_PEB64 peb64 = { 0 };
            _process.core().peb( &peb64 );
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
DWORD InjectionCore::ValidateContext( InjectContext& context, const blackbone::pe::PEImage& img )
{
    // Invalid path
    if (context.images.empty())
    {
        Message::ShowError( _hMainDlg, L"Please add at least one image to inject" );
        return ERROR_INVALID_PARAMETER;
    }

    // Validate driver
    if (context.injectMode == Kernel_DriverMap)
    {
        // Only x64 drivers are supported
        if (img.mType() != blackbone::mt_mod64)
        {
            Message::ShowError( _hMainDlg, L"Can't map x86 drivers - '" + img.name() + L"'" );
            return ERROR_INVALID_IMAGE_HASH;
        }

        // Image must be native
        if (img.subsystem() != IMAGE_SUBSYSTEM_NATIVE)
        {
            Message::ShowError( _hMainDlg, L"Can't map image with non-native subsystem - '" + img.name() + L"'" );
            return ERROR_INVALID_IMAGE_HASH;
        }

        return ERROR_SUCCESS;
    }

    // No process selected
    if (!_process.valid())
    {
        Message::ShowError( _hMainDlg, L"Please select valid process before injection" );
        return ERROR_INVALID_HANDLE;
    }

    auto& barrier = _process.core().native()->GetWow64Barrier();

    // Validate architecture
    if (!img.IsPureManaged() && img.mType() == blackbone::mt_mod32 && barrier.targetWow64 == false)
    {
        Message::ShowError( _hMainDlg, L"Can't inject 32 bit image '" + img.name() + L"' into native 64 bit process" );
        return ERROR_INVALID_IMAGE_HASH;
    }

    // Additional validation for kernel manual map
    if (context.injectMode == Kernel_MMap && !img.IsPureManaged() && img.mType() == blackbone::mt_mod64 && barrier.targetWow64 == true)
    {
        Message::ShowError( _hMainDlg, L"Can't inject 64 bit image '" + img.name() + L"' into WOW64 process" );
        return ERROR_INVALID_PARAMETER;
    }

    // Can't inject managed dll through WOW64 barrier
    if (img.IsPureManaged() && (barrier.type == blackbone::wow_32_64 || barrier.type == blackbone::wow_64_32))
    {
        if (barrier.type == blackbone::wow_32_64)
            Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to inject managed dll '" + img.name() + L"' into x64 process" );
        else
            Message::ShowWarning( _hMainDlg, L"Please use Xenos.exe to inject managed dll '" + img.name() + L"' into WOW64 process" );

        return ERROR_INVALID_PARAMETER;
    }

    // Can't inject 64 bit image into WOW64 process from x86 version
    if (img.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_32_32)
    {
        Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to inject 64 bit image '" + img.name() + L"' into WOW64 process" );
        return ERROR_INVALID_PARAMETER;
    }

    // Can't execute code in another thread trough WOW64 barrier
    if (context.hijack && barrier.type != blackbone::wow_32_32 &&  barrier.type != blackbone::wow_64_64)
    {
        Message::ShowError( _hMainDlg, L"Can't execute code in existing thread trough WOW64 barrier" );
        return ERROR_INVALID_PARAMETER;
    }

    // Manual map restrictions
    if (context.injectMode == Manual || context.injectMode == Kernel_MMap)
    {
        if (img.IsPureManaged())
        {
            Message::ShowError( _hMainDlg, L"Pure managed image '" + img.name() + L"' can't be manually mapped yet" );
            return ERROR_INVALID_PARAMETER;
        }

        if (((img.mType() == blackbone::mt_mod32 && barrier.sourceWow64 == false) ||
            (img.mType() == blackbone::mt_mod64 && barrier.sourceWow64 == true)))
        {
            if (img.mType() == blackbone::mt_mod32)
                Message::ShowWarning( _hMainDlg, L"Please use Xenos.exe to manually map 32 bit image '" + img.name() + L"'" );
            else
                Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to manually map 64 bit image '" + img.name() + L"'" );

            return ERROR_INVALID_PARAMETER;
        }
    }

    // Trying to inject x64 dll into WOW64 process
    if (img.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_64_32)
    {
        int btn = MessageBoxW(
            _hMainDlg,
            (L"Are you sure you want to inject 64 bit dll '" + img.name() + L"' into WOW64 process?").c_str(),
            L"Warning",
            MB_ICONWARNING | MB_YESNO
            );

        // Canceled by user
        if (btn != IDYES)
            return ERROR_CANCELLED;
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Validate initialization routine
/// </summary>
/// <param name="init">Routine name</param>
/// <param name="initRVA">Routine RVA, if found</param>
/// <returns>Error code</returns>
DWORD InjectionCore::ValidateInit( const std::string& init, uint32_t& initRVA, blackbone::pe::PEImage& img )
{
    // Validate init routine
    if (img.IsPureManaged())
    {
        blackbone::ImageNET::mapMethodRVA methods;
        img.net().Parse( methods );
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
                Message::ShowError( _hMainDlg, L"Please select '" + img.name() + L"' entry point" );
            else
            {
                auto str = blackbone::Utils::FormatString(
                    L"Image '%ls' does not contain specified method - '%ls'",
                    img.name().c_str(),
                    blackbone::Utils::AnsiToWstring( init.c_str() ).c_str()
                    );

                Message::ShowError( _hMainDlg, str );
            }

            return ERROR_NOT_FOUND;

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
            return ERROR_NOT_FOUND;
        }
        else
        {
            initRVA = iter->RVA;
        }
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Inject multiple images
/// </summary>
/// <param name="pCtx">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::InjectMultiple( InjectContext* pContext )
{
    DWORD errCode = ERROR_SUCCESS;
    PROCESS_INFORMATION pi = { 0 };

    // Log some info
    xlog::Critical(
        "Injection initiated. Mode: %d, process type: %d, pid: %d, mmap flags: 0x%X, "
        "erasePE: %d, unlink: %d, thread hijack: %d, init routine: '%s', init arg: '%ls'",
        pContext->injectMode, 
        pContext->procMode,
        pContext->pid,
        pContext->flags,
        pContext->erasePE,
        pContext->unlinkImage,
        pContext->hijack,
        pContext->initRoutine.c_str(),
        pContext->initRoutineArg.c_str()
        );

    // Get process for injection
    errCode = GetTargetProcess( *pContext, pi );
    if (errCode != ERROR_SUCCESS)
        return errCode;

    if (pContext->delay)
        Sleep( pContext->delay );

    // Inject all images
    for (auto& img : pContext->images)
    {
        errCode |= InjectSingle( *pContext, img );
        if (pContext->period)
            Sleep( pContext->period );
    }

    //
    // Cleanup after injection
    //
    if (pi.hThread)
    {
        if (errCode == ERROR_SUCCESS)
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
    if (errCode == ERROR_SUCCESS && (pContext->flags & blackbone::HideVAD) && 
         (pContext->injectMode == Manual || pContext->injectMode == Kernel_MMap))
    {
        _criticalProcList.emplace_back( _process.pid() );
    }

    if (_process.core().handle())
        _process.Detach();

    return errCode;
}


/// <summary>
/// Injector thread worker
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::InjectSingle( InjectContext& context, blackbone::pe::PEImage& img )
{
    DWORD errCode = ERROR_SUCCESS;
    blackbone::Thread *pThread = nullptr;
    const blackbone::ModuleData* mod = nullptr;
    uint32_t exportRVA = 0;

    xlog::Critical( "Injecting image '%ls'", img.path().c_str() );

    // Check export
    errCode = ValidateInit( context.initRoutine, exportRVA, img );
    if (errCode != ERROR_SUCCESS)
    {
        xlog::Error( "Image init routine check failed, status: 0x%X", errCode );
        return errCode;
    }

    // Final sanity check
    if (context.injectMode < Kernel_Thread || context.injectMode == Kernel_DriverMap)
    {
        errCode = ValidateContext( context, img );
        if (errCode != ERROR_SUCCESS)
        {
            xlog::Error( "Context validation failed, status: 0x%X", errCode );
            return errCode;
        }
    }

    // Get context thread
    if (context.hijack && context.injectMode < Kernel_Thread)
    {
        xlog::Normal( "Searching for thread to hijack" );
        pThread = _process.threads().getMostExecuted();
        if (pThread == nullptr)
        {
            Message::ShowError( _hMainDlg, L"Failed to get suitable thread for execution");
            return errCode = ERROR_NOT_FOUND;
        }
    }

    // Actual injection
    if (errCode == ERROR_SUCCESS)
    {
        switch (context.injectMode)
        {
            case Normal:
                errCode = InjectDefault( context, img, pThread, mod );
                break;

            case Manual:
                mod = _process.mmap().MapImage( img.path(), blackbone::RebaseProcess | blackbone::NoDelayLoad | context.flags );
                if (!mod)
                    xlog::Error( "Failed to inject image using manual map, status: 0x%X", LastNtStatus() );
                break;

            case Kernel_Thread:
            case Kernel_APC:
            case Kernel_MMap:
                errCode = InjectKernel( context, img,  exportRVA );
                if (!NT_SUCCESS( errCode ))
                    xlog::Error( "Failed to inject image using kernel injection, status: 0x%X", errCode );
                break;                

            case Kernel_DriverMap:
                errCode = MapDriver( context, img );
                break;

            default:
                break;
        }
    }

    // Fix error code
    if (!img.IsPureManaged() && mod == nullptr && context.injectMode < Kernel_Thread && errCode == ERROR_SUCCESS)
        errCode = STATUS_UNSUCCESSFUL;

    // Initialize routine
    if (errCode == ERROR_SUCCESS && context.injectMode < Kernel_Thread)
    {
        errCode = CallInitRoutine( context, img, mod, exportRVA, pThread );
    }
    else if (errCode != ERROR_SUCCESS)
    {
        wchar_t errBuf[128] = { 0 };
        wsprintfW( errBuf, L"Failed to inject image '%ls'.\nError code 0x%X", img.path().c_str(), errCode );
        Message::ShowError( _hMainDlg, errBuf );
    }

    // Erase header
    if (errCode == ERROR_SUCCESS && mod && context.injectMode == Normal && context.erasePE)
    {
        auto base = mod->baseAddress;
        auto size = img.headersSize();
        DWORD oldProt = 0;
        std::unique_ptr<uint8_t> zeroBuf( new uint8_t[size]() );

        _process.memory().Protect( mod->baseAddress, size, PAGE_EXECUTE_READWRITE, &oldProt );
        _process.memory().Write( base, size, zeroBuf.get() );
        _process.memory().Protect( mod->baseAddress, size, oldProt );
    }

    // Unlink module if required
    if (errCode == ERROR_SUCCESS && mod && context.injectMode == Normal && context.unlinkImage)
        if (_process.modules().Unlink( mod ) == false)
        {
            errCode = ERROR_FUNCTION_FAILED;
            Message::ShowError( _hMainDlg, L"Failed to unlink module '" + img.path() + L"'" );
        }
    
    return errCode;
}


/// <summary>
/// Default injection method
/// </summary>
/// <param name="context">Injection context</param>
/// <param name="pThread">Context thread of execution</param>
/// <param name="mod">Resulting module</param>
/// <returns>Error code</returns>
DWORD InjectionCore::InjectDefault(
    InjectContext& context, 
    const blackbone::pe::PEImage& img,
    blackbone::Thread* pThread,
    const blackbone::ModuleData* &mod
    )
{
    // Pure IL image
    if (img.IsPureManaged())
    {
        DWORD code = 0;

        xlog::Normal( "Image '%ls' is pure IL", img.path().c_str() );

        if (!_process.modules().InjectPureIL(
            blackbone::ImageNET::GetImageRuntimeVer( img.path().c_str() ),
            img.path(),
            blackbone::Utils::AnsiToWstring( context.initRoutine ),
            context.initRoutineArg,
            code ))
        {
            xlog::Error( "Failed to inject pure IL image, status: %d", code );
            return ERROR_FUNCTION_FAILED;
        }

        mod = _process.modules().GetModule( img.name(), blackbone::Sections );
        return ERROR_SUCCESS;
    }
    // Inject through existing thread
    else if (pThread != nullptr)
    {
        // Load 
        auto pLoadLib = _process.modules().GetExport( _process.modules().GetModule( L"kernel32.dll" ), "LoadLibraryW" ).procAddress;
        blackbone::RemoteFunction<decltype(&LoadLibraryW)> pfn( _process, (decltype(&LoadLibraryW))pLoadLib, img.path().c_str() );
        
        decltype(pfn)::ReturnType junk = 0;
        pfn.Call( junk, pThread );

        if (junk == nullptr)
            xlog::Error( "Failed to inject image using thread hijack" );

        mod = _process.modules().GetModule( const_cast<const std::wstring&>(img.path()) );
    }
    else
    {
        NTSTATUS status = STATUS_SUCCESS;
        mod = _process.modules().Inject( img.path(), &status );
        if (!NT_SUCCESS( status ))
            xlog::Error( "Failed to inject image using default injection, status: 0x%X", status );
    }

    return mod != nullptr ? ERROR_SUCCESS : ERROR_FUNCTION_FAILED;
}

/// <summary>
/// Kernel-mode injection
/// </summary>
/// <param name="context">Injection context</param>
/// <param name="img">Target image</param>
/// <param name="initRVA">Init function RVA</param>
DWORD InjectionCore::InjectKernel(
    InjectContext& context,
    const blackbone::pe::PEImage& img,
    uint32_t initRVA /*= 0*/
    )
{
    if (context.injectMode == Kernel_MMap)
    {
        return blackbone::Driver().MmapDll(
            context.pid,
            img.path(),
            (KMmapFlags)context.flags,
            initRVA,
            context.initRoutineArg
            );
    }
    else
    {
        return blackbone::Driver().InjectDll(
            context.pid,
            img.path(),
            (context.injectMode == Kernel_Thread ? IT_Thread : IT_Apc),
            initRVA,
            context.initRoutineArg,
            context.unlinkImage,
            context.erasePE
            );
    }
}

/// <summary>
/// Manually map another system driver into system space
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::MapDriver( InjectContext& context, const blackbone::pe::PEImage& img )
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
DWORD InjectionCore::CallInitRoutine(
    InjectContext& context,
    const blackbone::pe::PEImage& img,
    const blackbone::ModuleData* mod,
    uint64_t exportRVA,
    blackbone::Thread* pThread
    )
{
    // Call init for native image
    if (!context.initRoutine.empty() && !img.IsPureManaged() && context.injectMode < Kernel_Thread)
    {
        auto fnPtr = mod->baseAddress + exportRVA;

        xlog::Normal( "Calling initialization routine for image '%ls'", img.path().c_str() );

        // Create new thread
        if (pThread == nullptr)
        {
            auto argMem = _process.memory().Allocate( 0x1000, PAGE_READWRITE );
            argMem.Write( 0, context.initRoutineArg.length() * sizeof( wchar_t ) + 2, context.initRoutineArg.c_str() );

            xlog::Normal( "Initialization routine returned 0x%X", _process.remote().ExecDirect( fnPtr, argMem.ptr() ) );
        }
        // Execute in existing thread
        else
        {
            blackbone::RemoteFunction<fnInitRoutine> pfn( _process, (fnInitRoutine)fnPtr, context.initRoutineArg.c_str() );

            int junk = 0;
            pfn.Call( junk, pThread );

            xlog::Normal( "Initialization routine returned 0x%X", junk );
        }
    }

    return ERROR_SUCCESS;
}

