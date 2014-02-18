#include "MainWnd.h"

#include "../../BlackBone/src/BlackBone/RemoteFunction.hpp"

/// <summary>
/// Attach to selected process
/// </summary>
/// <param name="pid">The pid.</param>
/// <returns>Error code</returns>
DWORD MainDlg::AttachToProcess( DWORD pid )
{
    NTSTATUS status = _proc.Attach( pid );

    if (status != STATUS_SUCCESS)
    {
        std::wstring errmsg = L"Can not attach to process.\n" + blackbone::Utils::GetErrorDescription( status );
        MessageBoxW( _hMainDlg, errmsg.c_str(), L"Error", MB_ICONERROR );
        return status;
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Enumerate processes
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::FillProcessList()
{
    PROCESSENTRY32W pe32 = { 0 };
    pe32.dwSize = sizeof(pe32);

    HWND hCombo = GetDlgItem( _hMainDlg, IDC_COMBO_PROC );

    ComboBox_ResetContent( hCombo );

    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (hSnap == NULL)
        return GetLastError();

    for (BOOL res = Process32FirstW( hSnap, &pe32 ); res; res = Process32NextW( hSnap, &pe32 ))
    {
        wchar_t text[255] = { 0 };
        swprintf_s( text, L"%ls (%d)", pe32.szExeFile, pe32.th32ProcessID );

        int idx = ComboBox_AddString( hCombo, text );
        ComboBox_SetItemData( hCombo, idx, pe32.th32ProcessID );
    }

    CloseHandle( hSnap );

    return 0;
}

/// <summary>
/// Retrieve process threads
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::FillThreads()
{
    HWND hCombo = GetDlgItem( _hMainDlg, IDC_THREADS );
    int idx = 0;

    ComboBox_ResetContent( hCombo );

    auto tMain = _proc.threads().getMain();
    if (!tMain)
        return ERROR_NOT_FOUND;

    // Fake 'New thread'
    idx = ComboBox_AddString( hCombo, L"New thread" );
    ComboBox_SetItemData( hCombo, idx, 0 );
    ComboBox_SetCurSel( hCombo, idx );

    for (auto& thd : _proc.threads().getAll( true ))
    {
        wchar_t text[255] = { 0 };

        if (thd == *tMain)
            swprintf_s( text, L"Thread %d (Main)", thd.id() );
        else
            swprintf_s( text, L"Thread %d", thd.id() );

        idx = ComboBox_AddString( hCombo, text );
        ComboBox_SetItemData( hCombo, idx, thd.id() );
    }


    return 0;
}

/// <summary>
/// Load selected image and do some validation
/// </summary>
DWORD MainDlg::SetActiveProcess( bool createNew, const wchar_t* path, DWORD pid /*= 0xFFFFFFFF*/ )
{
    HWND hCombo = GetDlgItem( _hMainDlg, IDC_COMBO_PROC );

    if (createNew)
    {
        std::wstring procName = blackbone::Utils::StripPath( path ) + L" (New process)";     

        // Update process list
        auto idx = ComboBox_AddString( hCombo, procName.c_str() );
        ComboBox_SetItemData( hCombo, idx, -1 );
        ComboBox_SetCurSel( hCombo, idx );

        // Enable command line options field
        EnableWindow( GetDlgItem( _hMainDlg, IDC_CMDLINE ), TRUE );

        _newProcess = true;
        _procPath = path;
    }
    else if (pid != 0xFFFFFFFF && AttachToProcess( pid ) == ERROR_SUCCESS)
    {
        FillThreads();

        _newProcess = false;

        if (path != nullptr)
        {
            std::wstring procName = std::wstring( path ) + L" (" + std::to_wstring( _proc.pid() ) + L")";
            _procPath = path;

            auto idx = ComboBox_AddString( hCombo, procName.c_str() );
            ComboBox_SetItemData( hCombo, idx, -1 );
            ComboBox_SetCurSel( hCombo, idx );
        }
        else
            _procPath = _proc.modules().GetMainModule()->name;

        // Disable command line option field
        EnableWindow( GetDlgItem( _hMainDlg, IDC_CMDLINE ), FALSE );
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Load selected image and do some validation
/// </summary>
/// <param name="path">Full qualified image path</param>
/// <returns>Error code</returns>
DWORD MainDlg::LoadImageFile( const wchar_t* path )
{
    HWND hCombo = GetDlgItem( _hMainDlg, IDC_INIT_EXPORT );

    ComboBox_ResetContent( hCombo );

    // Check if image is a PE file
    if (_file.Project( path ) == nullptr || _imagePE.Parse( _file.base(), _file.isPlainData() ) == false)
    {
        DWORD err = GetLastError();
        std::wstring errstr = std::wstring( L"File \"" ) + path + L"\" is not a valid PE image";

        Edit_SetText( GetDlgItem( _hMainDlg, IDC_IMAGE_PATH ), L"" );
        MessageBoxW( _hMainDlg, errstr.c_str(), L"Error", MB_ICONERROR );

        _file.Release();
        return err;
    }

    // In case of pure IL, list all methods
    if (_imagePE.IsPureManaged( ))
    {
        if (_imagePE.net().Init( path ))
        {
            blackbone::ImageNET::mapMethodRVA methods;
            _imagePE.net().Parse( methods );

            for (auto& entry : methods)
            {
                std::wstring name = entry.first.first + L"." + entry.first.second;
                SendMessageW( hCombo, CB_ADDSTRING, 0L, (LPARAM)name.c_str() );
            }
        }
    }
    // Simple exports otherwise
    else
    {
        std::list<std::string> names;
        _imagePE.GetExportNames( names );

        for (auto& name : names)
            SendMessageA( hCombo, CB_ADDSTRING, 0L, (LPARAM)name.c_str() );
    }

    return 0;
}

/// <summary>
/// Get manual map flags from interface
/// </summary>
/// <returns>Flags</returns>
DWORD MainDlg::MmapFlags()
{
    DWORD flags = 0;

    if (Button_GetCheck( GetDlgItem( _hMainDlg, IDC_MANUAL_IMP ) ))
        flags |= blackbone::ManualImports;

    if (Button_GetCheck( GetDlgItem( _hMainDlg, IDC_LDR_REF ) ))
        flags |= blackbone::CreateLdrRef;

    if (Button_GetCheck( GetDlgItem( _hMainDlg, IDC_WIPE_HDR ) ))
        flags |= blackbone::WipeHeader;

    if (Button_GetCheck( GetDlgItem( _hMainDlg, IDC_IGNORE_TLS ) ))
        flags |= blackbone::NoTLS;

    if (Button_GetCheck( GetDlgItem( _hMainDlg, IDC_NOEXCEPT ) ))
        flags |= blackbone::NoExceptions;

    return flags;
}

/// <summary>
/// Update interface manual map flags
/// </summary>
/// <param name="flags">Flags</param>
/// <returns>Flags</returns>
DWORD MainDlg::MmapFlags( DWORD flags )
{
    Button_SetCheck( GetDlgItem( _hMainDlg, IDC_MANUAL_IMP ),   flags & blackbone::ManualImports );
    Button_SetCheck( GetDlgItem( _hMainDlg, IDC_LDR_REF ),      flags & blackbone::CreateLdrRef );
    Button_SetCheck( GetDlgItem( _hMainDlg, IDC_WIPE_HDR ),     flags & blackbone::WipeHeader );
    Button_SetCheck( GetDlgItem( _hMainDlg, IDC_IGNORE_TLS ),   flags & blackbone::NoTLS );
    Button_SetCheck( GetDlgItem( _hMainDlg, IDC_NOEXCEPT ),     flags & blackbone::NoExceptions );

    return flags;
}


/// <summary>
/// Set injection method
/// </summary>
/// <param name="mode">Injection mode</param>
/// <returns>Error code</returns>
DWORD MainDlg::SetMapMode( MapMode mode )
{
    if (mode == Normal)
    {
        EnableWindow( GetDlgItem( _hMainDlg, IDC_THREADS ), TRUE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_UNLINK ), TRUE );

        EnableWindow( GetDlgItem( _hMainDlg, IDC_WIPE_HDR ), FALSE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_LDR_REF ), FALSE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_MANUAL_IMP ), FALSE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_IGNORE_TLS ), FALSE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_NOEXCEPT ), FALSE );
    }
    else if( mode == Manual )
    {
        ComboBox_SetCurSel( GetDlgItem( _hMainDlg, IDC_THREADS ), 0 );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_THREADS ), FALSE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_UNLINK ), FALSE );

        EnableWindow( GetDlgItem( _hMainDlg, IDC_WIPE_HDR ), TRUE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_LDR_REF ), TRUE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_MANUAL_IMP ), TRUE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_IGNORE_TLS ), TRUE );
        EnableWindow( GetDlgItem( _hMainDlg, IDC_NOEXCEPT ), TRUE );
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Validate all parameters
/// </summary>
/// <param name="path">Image path</param>
/// <param name="barrier">Process WOW64 info</param>
/// <param name="thdId">Context thread id.</param>
/// <param name="isManual">true if image is itended to be manually mapped</param>
/// <returns>Error code</returns>
DWORD MainDlg::ValidateArch( const wchar_t* path, const blackbone::Wow64Barrier &barrier, DWORD thdId, bool isManual )
{
    // No process selected
    if (!_proc.valid())
    {
        MessageBoxW( _hMainDlg, L"Please select valid process before injection", L"Error", MB_ICONERROR );
        return ERROR_INVALID_HANDLE;
    }

    // Invalid path
    if (path == nullptr || path[0] == 0)
    {
        MessageBoxW( _hMainDlg, L"Please select image to inject", L"Error", MB_ICONERROR );
        return ERROR_INVALID_PARAMETER;
    }

    // Validate architecture
    if (!_imagePE.IsPureManaged() && _imagePE.mType() == blackbone::mt_mod32 && barrier.targetWow64 == false)
    {
        MessageBoxW( _hMainDlg, L"Can't inject 32 bit image into native 64 bit process", L"Error", MB_ICONERROR );
        return ERROR_INVALID_IMAGE_HASH;
    }
    // Can't inject managed dll through WOW64 barrier
    else if (_imagePE.IsPureManaged() && (barrier.type == blackbone::wow_32_64 || barrier.type == blackbone::wow_64_32))
    {
        if (barrier.type == blackbone::wow_32_64)
            MessageBoxW( _hMainDlg, L"Please use Xenos64.exe to inject managed dll into x64 process", L"Error", MB_ICONERROR );
        else
            MessageBoxW( _hMainDlg, L"Please use Xenos.exe to inject managed dll into WOW64 process", L"Error", MB_ICONERROR );

        return ERROR_INVALID_IMAGE_HASH;
    }
    else if (_imagePE.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_32_32)
    {
        MessageBoxW( _hMainDlg, L"Please use Xenos64.exe to inject 64 bit image into WOW64 process", L"Error", MB_ICONERROR );
        return ERROR_INVALID_IMAGE_HASH;
    }
    // Can't execute code in another thread trough WOW64 barrier
    else if (thdId != 0 && barrier.type != blackbone::wow_32_32 &&  barrier.type != blackbone::wow_64_64)
    {
        MessageBoxW( _hMainDlg, L"Can't execute code in existing thread trough WOW64 barrier", L"Error", MB_ICONERROR );
        return ERROR_INVALID_IMAGE_HASH;
    }
    // Manual map restrictions
    else if (isManual && ((_imagePE.mType() == blackbone::mt_mod32 && barrier.sourceWow64 == false) ||
        (_imagePE.mType() == blackbone::mt_mod64 && barrier.sourceWow64 == true)))
    {
        if (_imagePE.mType() == blackbone::mt_mod32)
            MessageBoxW( _hMainDlg, L"Please use Xenos.exe to map 32 bit image", L"Error", MB_ICONERROR );
        else
            MessageBoxW( _hMainDlg, L"Please use Xenos64.exe to map 64 bit image", L"Error", MB_ICONERROR );

        return ERROR_INVALID_IMAGE_HASH;
    }
    else if (isManual && _imagePE.IsPureManaged())
    {
        MessageBoxW( _hMainDlg, L"Pure managed images can't be manually mapped yet", L"Error", MB_ICONERROR );

        return ERROR_INVALID_IMAGE_HASH;
    }
    // Trying to inject x64 dll into WOW64 process
    else if (_imagePE.mType() == blackbone::mt_mod64 && barrier.type == blackbone::wow_64_32)
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
/// <returns>Error code</returns>
DWORD MainDlg::ValidateInit( const char* init )
{
    // Validate init routine
    if (_imagePE.IsPureManaged())
    {
        blackbone::ImageNET::mapMethodRVA methods;
        _imagePE.net().Parse( methods );
        bool found = false;

        if (!methods.empty() && init && init[0] != 0)
        {
            std::wstring winit = blackbone::Utils::AnsiToWstring( init );

            for (auto& val : methods)
            if (winit == (val.first.first + L"." + val.first.second))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            MessageBoxW( _hMainDlg, L"Image does not contain specified method", L"Error", MB_ICONERROR );
            return ERROR_NOT_FOUND;
        }
    }
    else if (init && init[0] != 0)
    {
        std::list<std::string> names;
        _imagePE.GetExportNames( names );

        auto iter = std::find( names.begin(), names.end(), init );
        if (iter == names.end())
        {
            MessageBoxW( _hMainDlg, L"Image does not contain specified export", L"Error", MB_ICONERROR );
            return ERROR_NOT_FOUND;
        }
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Validate image before injection
/// </summary>
/// <param name="path">Image path</param>
/// <param name="init">Initizliation routine</param>
/// <returns></returns>
DWORD MainDlg::ValidateImage( const wchar_t* path, const char* init )
{
    DWORD status  = ERROR_SUCCESS;
    auto& barrier = _proc.core().native()->GetWow64Barrier();
    DWORD thdId = _newProcess ? 0 : (DWORD)ComboBox_GetItemData( GetDlgItem( _hMainDlg, IDC_THREADS ), 
                                                                 ComboBox_GetCurSel( GetDlgItem( _hMainDlg, IDC_THREADS ) ) );

    bool isManual = (ComboBox_GetCurSel( GetDlgItem( _hMainDlg, IDC_OP_TYPE ) ) == 1);

    return ValidateArch( path, barrier, thdId, isManual );
}

/// <summary>
/// Initiate injection process
/// </summary>
/// <param name="path">Image path</param>
/// <param name="init">Initizliation routine</param>
/// <param name="arg">Initizliation routine argument</param>
/// <returns>Error code</returns>
DWORD MainDlg::DoInject( const wchar_t* path, const char* init, const wchar_t* arg )
{
    _path = path;
    _init = init;
    _arg = arg;

    CreateThread( NULL, 0, &MainDlg::InjectWrap, this, 0, NULL );

    return ERROR_SUCCESS;
}

/// <summary>
/// Injector thread wrapper
/// </summary>
/// <param name="lpPram">this pointer</param>
/// <returns>Error code</returns>
DWORD MainDlg::InjectWrap( LPVOID lpPram )
{
    MainDlg* pThis = (MainDlg*)lpPram;
    return pThis->InjectWorker( pThis->_path, pThis->_init, pThis->_arg );
}

/// <summary>
/// Injection routine
/// </summary>
/// <param name="path">Image path</param>
/// <param name="init">Initizliation routine/param>
/// <param name="arg">Initizliation routine argument</param>
/// <returns>Error code</returns>
DWORD MainDlg::InjectWorker( std::wstring path, std::string init, std::wstring arg )
{
    blackbone::Thread *pThread = nullptr;
    const blackbone::ModuleData* mod = nullptr;
    PROCESS_INFORMATION pi = { 0 };
    wchar_t cmdline[256] = { 0 };
    HWND hCombo = GetDlgItem( _hMainDlg, IDC_THREADS );
    DWORD thdId = (DWORD)ComboBox_GetItemData( hCombo, ComboBox_GetCurSel( hCombo ) );
    bool bManual = ComboBox_GetCurSel( GetDlgItem( _hMainDlg, IDC_OP_TYPE ) ) == 1;
    
    GetDlgItemTextW( _hMainDlg, IDC_CMDLINE, cmdline, ARRAYSIZE( cmdline ) );  

    // Check export
    if (ValidateInit( init.c_str() ) != STATUS_SUCCESS)
        return ERROR_CANCELLED;

    // Create new process
    if (_newProcess)
    {
        STARTUPINFOW si = { 0 };
        si.cb = sizeof(si);

        if (!CreateProcessW( _procPath.c_str(), cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            MessageBoxW( _hMainDlg, L"Failed to create new process", L"Error", MB_ICONERROR );
            return GetLastError();
        }

        thdId = 0;

        // Wait for process to initialize loader
        Sleep( 1 );

        AttachToProcess( pi.dwProcessId );
    }

    // Final sanity check
    if (ValidateImage( path.c_str(), init.c_str() ) != ERROR_SUCCESS)
    {
        if (_newProcess)
            TerminateProcess( pi.hProcess, 0 );

        return ERROR_CANCELLED;
    }
        
    // Normal inject
    if (bManual == false)
    {
        if (_imagePE.IsPureManaged())
        {
            DWORD code = 0;

            if (!_proc.modules().InjectPureIL( blackbone::ImageNET::GetImageRuntimeVer( path.c_str() ),
                path, blackbone::Utils::AnsiToWstring( init ), arg, code ))
            {
                if (_newProcess)
                    TerminateProcess( pi.hProcess, 0 );

                MessageBoxW( _hMainDlg, L"Failed to inject image", L"Error", MB_ICONERROR );
                return ERROR_FUNCTION_FAILED;
            }
        }
        else if (!_newProcess && thdId != 0)
        {
            pThread = _proc.threads().get( thdId );
            if (pThread == nullptr)
            {
                if (_newProcess)
                    TerminateProcess( pi.hProcess, 0 );

                MessageBoxW( _hMainDlg, L"Selected thread does not exist", L"Error", MB_ICONERROR );
                return ERROR_NOT_FOUND;
            }

            // Load 
            auto pLoadLib = _proc.modules().GetExport( _proc.modules().GetModule( L"kernel32.dll" ), "LoadLibraryW" ).procAddress;
            blackbone::RemoteFunction<decltype(&LoadLibraryW)> pfn( _proc, (decltype(&LoadLibraryW))pLoadLib, path.c_str() );
            decltype(pfn)::ReturnType junk = 0;
            pfn.Call( junk, pThread );

            mod = _proc.modules().GetModule( path );
        }
        else
            mod = _proc.modules().Inject( path );
    }
    // Manual map
    else
    {
        thdId = 0;
        int flags = blackbone::RebaseProcess | blackbone::NoDelayLoad | MmapFlags();

        mod = _proc.mmap().MapImage( path, flags );
    }

    if (mod == 0 && !_imagePE.IsPureManaged())
    {
        if (_newProcess)
            TerminateProcess( pi.hProcess, 0 );

        MessageBoxW( _hMainDlg, L"Failed to inject image", L"Error", MB_ICONERROR );
        return ERROR_NOT_FOUND;
    }

    // Call init for native image
    if (!init.empty() && !_imagePE.IsPureManaged())
    {
        auto fnPtr = _proc.modules().GetExport( mod, init.c_str() ).procAddress;

        if (thdId == 0)
        {
            auto argMem = _proc.memory().Allocate( 0x1000, PAGE_READWRITE );
            argMem.Write( 0, arg.length() * sizeof(wchar_t)+2, arg.c_str() );

            _proc.remote().ExecDirect( fnPtr, argMem.ptr() );
        }
        else
        {
            pThread = _proc.threads().get( thdId );
            if (pThread == nullptr)
            {
                if (_newProcess)
                    TerminateProcess( pi.hProcess, 0 );

                MessageBoxW( _hMainDlg, L"Selected thread does not exist", L"Error", MB_ICONERROR );
                return ERROR_NOT_FOUND;
            }

            blackbone::RemoteFunction<int( _stdcall* )(const wchar_t*)> pfn( _proc, (int( _stdcall* )(const wchar_t*))fnPtr, arg.c_str() );
            int junk = 0;

            pfn.Call( junk, pThread );
        }
    }

    // Unlink module if required
    if (!_imagePE.IsPureManaged() && !bManual && Button_GetCheck( GetDlgItem( _hMainDlg, IDC_UNLINK ) ))
        if (_proc.modules().Unlink( mod ) == false)
            MessageBoxW( _hMainDlg, L"Failed to unlink module", L"Error", MB_ICONERROR );

    // MessageBoxW( _hMainDlg, L"Successfully injected", L"Info", MB_ICONINFORMATION );
    //ResumeThread( pi.hThread );

    return ERROR_SUCCESS;
}