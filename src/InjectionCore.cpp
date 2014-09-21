#include "InjectionCore.h"

#include "../../BlackBone/src/BlackBone/Process/RPC/RemoteFunction.hpp"

InjectionCore::InjectionCore( HWND& hMainDlg )
    :_hMainDlg( hMainDlg )
{
    // Load driver
    blackbone::Driver().EnsureLoaded();
}


InjectionCore::~InjectionCore()
{
}


/// <summary>
/// Attach to selected process or create a new process
/// </summary>
/// <param name="pid">The pid.</param>
/// <returns>Error code</returns>
DWORD InjectionCore::CreateOrAttach( DWORD pid, InjectContext& context, PROCESS_INFORMATION& pi )
{
    NTSTATUS status = STATUS_SUCCESS;

    // No process required for driver mapping
    if (context.mode == Kernel_DriverMap)
    {
        context.threadID = 0;
        return status;
    }

    // Create new process
    if (pid == 0)
    {
        STARTUPINFOW si = { 0 };
        si.cb = sizeof( si );

        if (!CreateProcessW( context.procPath.c_str(), (LPWSTR)context.procCmdLine.c_str(), 
                             NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
        {
            Message::ShowError( _hMainDlg, L"Failed to create new process.\n" + blackbone::Utils::GetErrorDescription( GetLastError() ) );
            return GetLastError();
        }

        if (NT_SUCCESS( _proc.Attach( pi.dwProcessId, PROCESS_ALL_ACCESS ) ))
        {
            // Create new thread to make sure LdrInitializeProcess gets called
            auto& mods = _proc.modules();
            auto pProc = mods.GetExport( mods.GetModule( L"ntdll.dll", blackbone::Sections ), "NtYieldExecution" ).procAddress;
            if (pProc)
                _proc.remote().ExecDirect( pProc, 0 );
        }

        // No process handle is required for kernel injection
        if (context.mode >= Kernel_Thread)
        {
            context.pid = pi.dwProcessId;
            _proc.Detach();

            // Thread must be running for APC to execute
            if (context.mode == Kernel_APC)
            {
                ResumeThread( pi.hThread );
                Sleep( 100 );
            }
        }

        context.threadID = 0;
    }
    // Attach to existing process
    else if (context.mode < Kernel_Thread)
    {
        status = _proc.Attach( pid );
        if (status != STATUS_SUCCESS)
        {
            std::wstring errmsg = L"Can not attach to the process.\n" + blackbone::Utils::GetErrorDescription( status );
            Message::ShowError( _hMainDlg, errmsg );

            return status;
        }
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Load selected image and do some validation
/// </summary>
/// <param name="path">Full qualified image path</param>
/// <param name="exports">Image exports</param>
/// <returns>Error code</returns>
DWORD InjectionCore::LoadImageFile( const std::wstring& path, blackbone::pe::listExports& exports )
{
    // Check if image is a PE file
    if (!NT_SUCCESS( _imagePE.Load( path ) ))
    {
        std::wstring errstr = std::wstring( L"File \"" ) + path + L"\" is not a valid PE image";

        Message::ShowError( _hMainDlg, errstr.c_str() );

        _imagePE.Release();
        return ERROR_INVALID_IMAGE_HASH;
    }

    // In case of pure IL, list all methods
    if (_imagePE.IsPureManaged() && _imagePE.net().Init( path ))
    {
        blackbone::ImageNET::mapMethodRVA methods;
        _imagePE.net().Parse( methods );

        for (auto& entry : methods)
        {
            std::wstring name = entry.first.first + L"." + entry.first.second;
            exports.push_back( std::make_pair( blackbone::Utils::WstringToAnsi( name ), 0 ) );
        }
    }
    // Simple exports otherwise
    else
        _imagePE.GetExports( exports );

    return ERROR_SUCCESS;
}


/// <summary>
/// Validate all parameters
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::ValidateContext( const InjectContext& context )
{
    // Invalid path
    if (context.imagePath.empty())
    {
        Message::ShowError( _hMainDlg, L"Please select image to inject" );
        return ERROR_INVALID_PARAMETER;
    }

    // Validate driver
    if (context.mode == Kernel_DriverMap)
    {
        // Only x64 drivers are supported
        if (_imagePE.mType() != blackbone::mt_mod64)
        {
            Message::ShowError( _hMainDlg, L"Can't map x86 drivers" );
            return ERROR_INVALID_IMAGE_HASH;
        }

        // Image must be native
        if (_imagePE.subsystem() != IMAGE_SUBSYSTEM_NATIVE)
        {
            Message::ShowError( _hMainDlg, L"Can't map image with non-native subsystem" );
            return ERROR_INVALID_IMAGE_HASH;
        }

        return ERROR_SUCCESS;
    }

    // No process selected
    if (!_proc.valid())
    {
        Message::ShowError( _hMainDlg, L"Please select valid process before injection" );
        return ERROR_INVALID_HANDLE;
    }

    auto& barrier = _proc.core().native()->GetWow64Barrier();

    // Validate architecture
    if (!_imagePE.IsPureManaged() && _imagePE.mType() == blackbone::mt_mod32 && barrier.targetWow64 == false)
    {
        Message::ShowError( _hMainDlg, L"Can't inject 32 bit image into native 64 bit process" );
        return ERROR_INVALID_IMAGE_HASH;
    }

    // Can't inject managed dll through WOW64 barrier
    if (_imagePE.IsPureManaged() && (barrier.type == blackbone::wow_32_64 || barrier.type == blackbone::wow_64_32))
    {
        if (barrier.type == blackbone::wow_32_64)
            Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to inject managed dll into x64 process" );
        else
            Message::ShowWarning( _hMainDlg, L"Please use Xenos.exe to inject managed dll into WOW64 process" );

        return ERROR_INVALID_PARAMETER;
    }

    // Can't inject 64 bit image into WOW64 process from x86 version
    if (_imagePE.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_32_32)
    {
        Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to inject 64 bit image into WOW64 process" );
        return ERROR_INVALID_PARAMETER;
    }

    // Can't execute code in another thread trough WOW64 barrier
    if (context.threadID != 0 && barrier.type != blackbone::wow_32_32 &&  barrier.type != blackbone::wow_64_64)
    {
        Message::ShowError( _hMainDlg, L"Can't execute code in existing thread trough WOW64 barrier" );
        return ERROR_INVALID_PARAMETER;
    }

    // Manual map restrictions
    if (context.mode == Manual)
    {
        if (_imagePE.IsPureManaged())
        {
            Message::ShowError( _hMainDlg, L"Pure managed images can't be manually mapped yet" );
            return ERROR_INVALID_PARAMETER;
        }

        if (((_imagePE.mType() == blackbone::mt_mod32 && barrier.sourceWow64 == false) ||
            (_imagePE.mType() == blackbone::mt_mod64 && barrier.sourceWow64 == true)))
        {
            if (_imagePE.mType() == blackbone::mt_mod32)
                Message::ShowWarning( _hMainDlg, L"Please use Xenos.exe to manually map 32 bit image" );
            else
                Message::ShowWarning( _hMainDlg, L"Please use Xenos64.exe to manually map 64 bit image" );

            return ERROR_INVALID_PARAMETER;
        }        
    }

    // Trying to inject x64 dll into WOW64 process
    if (_imagePE.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_64_32)
    {
        int btn = MessageBoxW( _hMainDlg,
                               L"Are you sure you want to inject 64 bit dll into WOW64 process?",
                               L"Warning",
                               MB_ICONWARNING | MB_YESNO );

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
DWORD InjectionCore::ValidateInit( const std::string& init, uint32_t& initRVA )
{
    // Validate init routine
    if (_imagePE.IsPureManaged())
    {
        blackbone::ImageNET::mapMethodRVA methods;
        _imagePE.net().Parse( methods );
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
            Message::ShowError( _hMainDlg, L"Image does not contain specified method" );
            return ERROR_NOT_FOUND;
        }
    }
    else if (!init.empty())
    {
        blackbone::pe::listExports exports;
        _imagePE.GetExports( exports );

        auto iter = std::find_if(
            exports.begin(), exports.end(),
            [&init]( const blackbone::pe::listExports::value_type& val ){ return val.first == init; } );

        if (iter == exports.end())
        {
            initRVA = 0;
            Message::ShowError( _hMainDlg, L"Image does not contain specified export" );
            return ERROR_NOT_FOUND;
        }
        else
        {
            initRVA = iter->second;
        }
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Initiate injection process
/// </summary>
/// <param name="ctx">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::DoInject( InjectContext& ctx )
{
    ctx.pCore = this;

    // Save last context
    _context = ctx;

    _lastThread = CreateThread( NULL, 0, &InjectionCore::InjectWrapper, new InjectContext( ctx ), 0, NULL );
    return ERROR_SUCCESS;
}

/// <summary>
/// Waits for the injection thread to finish
/// </summary>
/// <returns>Injection status</returns>
DWORD InjectionCore::WaitOnInjection()
{
    DWORD code = ERROR_ACCESS_DENIED;

    WaitForSingleObject( _lastThread, INFINITE );
    GetExitCodeThread( _lastThread, &code );

    return code;
}

/// <summary>
/// Injector thread wrapper
/// </summary>
/// <param name="lpPram">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::InjectWrapper( LPVOID lpPram )
{
    InjectContext* pCtx = (InjectContext*)lpPram;
    return pCtx->pCore->InjectWorker( pCtx );
}

/// <summary>
/// Injector thread worker
/// </summary>
/// <param name="pCtx">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::InjectWorker( InjectContext* pCtx )
{
    DWORD errCode = ERROR_SUCCESS;
    blackbone::Thread *pThread = nullptr;
    const blackbone::ModuleData* mod = nullptr;
    PROCESS_INFORMATION pi = { 0 };
    uint32_t exportRVA = 0;

    // Check export
    errCode = ValidateInit( pCtx->initRoutine, exportRVA );
    if (errCode != ERROR_SUCCESS)
        return errCode;

    // Create new process
    errCode = CreateOrAttach( pCtx->pid, *pCtx, pi );
    if (errCode != ERROR_SUCCESS)
        return errCode;
        
    // Final sanity check
    if (pCtx->mode < Kernel_Thread || pCtx->mode == Kernel_DriverMap)
    {
        errCode = ValidateContext( *pCtx );
        if (errCode != ERROR_SUCCESS)
        {
            if (pCtx->pid == 0 && _proc.valid())
                _proc.Terminate();

            return errCode;
        }
    }

    // Get context thread
    if (pCtx->threadID != 0)
    {
        pThread = _proc.threads().get( pCtx->threadID );
        if (pThread == nullptr)
        {
            errCode = ERROR_NOT_FOUND;
            Message::ShowError( _hMainDlg, L"Selected thread does not exist" );
        }
    }

    // Actual injection
    if (errCode == ERROR_SUCCESS)
    {
        switch (pCtx->mode)
        {
            case Normal:
                errCode = InjectDefault( *pCtx, pThread, mod );
                break;

            case Manual:
                mod = _proc.mmap().MapImage( pCtx->imagePath, blackbone::RebaseProcess | blackbone::NoDelayLoad | pCtx->flags );
                break;

            case Kernel_Thread:
            case Kernel_APC:
                errCode = InjectKernel( *pCtx, mod, exportRVA );
                break;

            case Kernel_DriverMap:
                errCode = MapDriver( *pCtx );
                break;

            default:
                break;
        }
    }

    if (mod == nullptr)
        exportRVA = 0;

    // Initialize routine
    if (errCode == ERROR_SUCCESS)
    {
        errCode = CallInitRoutine( *pCtx, mod, exportRVA, pThread );
    }
    else
    {
        wchar_t errBuf[128] = { 0 };
        wsprintfW( errBuf, L"Failed to inject image.\nError code 0x%X", errCode );
        Message::ShowError( _hMainDlg, errBuf );
    }

    // Unlink module if required
    if (errCode == ERROR_SUCCESS && !_imagePE.IsPureManaged() && pCtx->mode == Normal && pCtx->unlinkImage)
        if (_proc.modules().Unlink( mod ) == false)
        {
            errCode = ERROR_FUNCTION_FAILED;
            Message::ShowError( _hMainDlg, L"Failed to unlink module" );
        }

    // Handle injection errors
    if (pi.hThread)
    {
        if (errCode == ERROR_SUCCESS)
            ResumeThread( pi.hThread );

        CloseHandle( pi.hThread );
    }

    if (pi.hProcess)
    {
        if (errCode != ERROR_SUCCESS)
            TerminateProcess( pi.hProcess, 0 );

        CloseHandle( pi.hProcess );
    }

    if (_proc.core().handle())
        _proc.Detach();
    
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
    blackbone::Thread* pThread,
    const blackbone::ModuleData* &mod )
{
    // Pure IL image
    if (_imagePE.IsPureManaged())
    {
        DWORD code = 0;

        if (!_proc.modules().InjectPureIL(
            blackbone::ImageNET::GetImageRuntimeVer( context.imagePath.c_str() ),
            context.imagePath,
            blackbone::Utils::AnsiToWstring( context.initRoutine ),
            context.initRoutineArg,
            code ))
        {
            return ERROR_FUNCTION_FAILED;
        }

        return ERROR_SUCCESS;
    }
    // Inject through existing thread
    else if (!context.pid == 0 && pThread != nullptr)
    {
        // Load 
        auto pLoadLib = _proc.modules().GetExport( _proc.modules().GetModule( L"kernel32.dll" ), "LoadLibraryW" ).procAddress;
        blackbone::RemoteFunction<decltype(&LoadLibraryW)> pfn( _proc, (decltype(&LoadLibraryW))pLoadLib, context.imagePath.c_str() );
        
        decltype(pfn)::ReturnType junk = 0;
        pfn.Call( junk, pThread );

        mod = _proc.modules().GetModule( const_cast<const std::wstring&>( context.imagePath ) );
    }
    else
        mod = _proc.modules().Inject( context.imagePath );

    return mod != nullptr ? ERROR_SUCCESS : ERROR_FUNCTION_FAILED;
}


/// <summary>
/// Kernel-mode injection
/// </summary>
/// <param name="context">Injection context</param>
/// <param name="mod">Resulting module</param>
/// <returns>Error code</returns>
DWORD InjectionCore::InjectKernel( InjectContext& context, const blackbone::ModuleData* &mod, uint32_t initRVA /*= 0*/ )
{
    auto status = blackbone::Driver().InjectDll(
        context.pid,
        context.imagePath,
        (context.mode == Kernel_Thread ? IT_Thread : IT_Apc),
        initRVA,
        context.initRoutineArg 
        );

    // Revert new process flag
    context.pid = 0;

    return status;
}

/// <summary>
/// Manually map another system driver into system space
/// </summary>
/// <param name="context">Injection context</param>
/// <returns>Error code</returns>
DWORD InjectionCore::MapDriver( InjectContext& context )
{
    return blackbone::Driver().MMapDriver( context.imagePath );
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
    const blackbone::ModuleData* mod,
    uint64_t exportRVA,
    blackbone::Thread* pThread
    )
{
    // Call init for native image
    if (!context.initRoutine.empty() && !_imagePE.IsPureManaged() && context.mode < Kernel_Thread)
    {
        auto fnPtr = mod->baseAddress + exportRVA;

        // Create new thread
        if (pThread == nullptr)
        {
            auto argMem = _proc.memory().Allocate( 0x1000, PAGE_READWRITE );
            argMem.Write( 0, context.initRoutineArg.length() * sizeof( wchar_t ) + 2, context.initRoutineArg.c_str() );

            _proc.remote().ExecDirect( fnPtr, argMem.ptr() );
        }
        // Execute in existing thread
        else
        {
            blackbone::RemoteFunction<fnInitRoutine> pfn( _proc, (fnInitRoutine)fnPtr, context.initRoutineArg.c_str() );

            int junk = 0;
            pfn.Call( junk, pThread );
        }
    }

    return ERROR_SUCCESS;
}

