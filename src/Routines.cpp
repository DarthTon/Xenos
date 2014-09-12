#include "MainWnd.h"


/// <summary>
/// Load configuration from file
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::LoadConfig()
{
    ConfigMgr::ConfigData cfg;
    if (_config.Load( cfg ))
    {
        // Image
        if (!cfg.imagePath.empty())
        {
            std::list<std::string> exportNames;

            if (_core.LoadImageFile( cfg.imagePath, exportNames ) == ERROR_SUCCESS)
            {
                _imagePath.text( cfg.imagePath );

                _initFuncList.reset();
                for (auto& name : exportNames)
                    _initFuncList.Add( name );
            }
        }

        // Process
        if (!cfg.procName.empty())
        {
            if (cfg.newProcess)
            {
                SetActiveProcess( 0, cfg.procName );
            }
            else
            {
                std::vector<DWORD> pidList;
                blackbone::Process::EnumByName( cfg.procName, pidList );

                if (!pidList.empty())
                {
                    auto idx = _procList.Add( cfg.procName + L" (" + std::to_wstring( pidList.front() ) + L")", pidList.front() );
                    _procList.selection( idx );

                    SetActiveProcess( pidList.front(), cfg.procName );
                }
            }
        }

        // Options
        _procCmdLine.text( cfg.procCmdLine );
        _initArg.text( cfg.initArgs );
        _initFuncList.text( cfg.initRoutine );

        _unlink.checked( cfg.unlink );
        _injClose.checked( cfg.close );

        // Injection type
        _injectionType.selection( cfg.injectMode );

        SetMapMode( (MapMode)cfg.injectMode );
        MmapFlags( (blackbone::eLoadFlags)cfg.manualMapFlags );
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Save Configuration.
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::SaveConfig()
{
    ConfigMgr::ConfigData cfg;

    auto thdId = _threadList.itemData( _threadList.selection() );

    cfg.newProcess     = _procList.itemData( _procList.selection() ) == 0;
    cfg.procName       = _processPath;
    cfg.imagePath      = _imagePath.text();
    cfg.procCmdLine    = _procCmdLine.text();
    cfg.initRoutine    = _initFuncList.selectedText();
    cfg.initArgs       = _initArg.text();
    cfg.injectMode     = _injectionType.selection();
    cfg.threadHijack   = (thdId != 0 && thdId != 0xFFFFFFFF);
    cfg.manualMapFlags = MmapFlags();
    cfg.unlink         = _unlink.checked();
    cfg.close          = _injClose.checked();

    _config.Save( cfg );

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

    _procList.reset( );

    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (hSnap == NULL)
        return GetLastError();

    for (BOOL res = Process32FirstW( hSnap, &pe32 ); res; res = Process32NextW( hSnap, &pe32 ))
    {
        wchar_t text[255] = { 0 };
        swprintf_s( text, L"%ls (%d)", pe32.szExeFile, pe32.th32ProcessID );

        _procList.Add( text, pe32.th32ProcessID );
    }

    CloseHandle( hSnap );
    return ERROR_SUCCESS;
}

/// <summary>
/// Retrieve process threads
/// </summary>
/// <param name="pid">Process ID</param>
/// <returns>Error code</returns>
DWORD MainDlg::FillThreads( DWORD pid )
{
    THREADENTRY32 te32 = { 0 };
    te32.dwSize = sizeof( te32 );
    uint64_t thdTime = 0xFFFFFFFFFFFFFFFF;
    DWORD mainThdId = 0;
    int mainThdIdx = 0;

    _threadList.reset();

    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if (hSnap == NULL)
        return GetLastError();

    _threadList.Add( L"New Thread" );

    for (BOOL res = Thread32First( hSnap, &te32 ); res; res = Thread32Next( hSnap, &te32 ))
    {
        if (te32.th32OwnerProcessID != pid)
            continue;

        int idx = _threadList.Add( std::to_wstring( te32.th32ThreadID ), te32.th32ThreadID );

        FILETIME times[4] = { 0 };
        /*HANDLE hThd = OpenThread( THREAD_QUERY_LIMITED_INFORMATION, FALSE, te32.th32ThreadID );

        if (hThd)
        {
            GetThreadTimes( hThd, &times[0], &times[1], &times[2], &times[3] );
            CloseHandle( hThd );

            auto time = ((static_cast<uint64_t>(times[2].dwHighDateTime) << 32) | times[2].dwLowDateTime)
                + ((static_cast<uint64_t>(times[3].dwHighDateTime) << 32) | times[3].dwLowDateTime);

            if (time < thdTime)
            {
                thdTime = time;
                mainThdId = te32.th32ThreadID;
                mainThdIdx = idx;
            }
        }*/
    }

    if (mainThdIdx != 0)
        _threadList.modifyItem( mainThdIdx, std::to_wstring( mainThdId ) + L" (Main)" );

    _threadList.selection( 0 );

    return ERROR_SUCCESS;
}

/// <summary>
/// Set current process
/// </summary>
/// <param name="pid">Target PID</param>
/// <param name="path">Process path.</param>
/// <returns>Error code</returns>
DWORD MainDlg::SetActiveProcess( DWORD pid, const std::wstring& path )
{
    _processPath = path;

    if (pid == 0)
    {
        // Update process list
        _procList.reset();
        _procList.Add( blackbone::Utils::StripPath( path ) + L" (New process)", 0 );
        _procList.selection( 0 );

        // Enable command line options field
        _procCmdLine.enable();
    }
    else
    {
        FillThreads( pid );

        // Disable command line option field
        _procCmdLine.disable();
    }

    return ERROR_SUCCESS;
}


/// <summary>
/// Get manual map flags from interface
/// </summary>
/// <returns>Flags</returns>
blackbone::eLoadFlags MainDlg::MmapFlags()
{
    blackbone::eLoadFlags flags = blackbone::NoFlags;

    if (_mmapOptions.manualInmport)
        flags |= blackbone::ManualImports;

    if (_mmapOptions.addLdrRef)
        flags |= blackbone::CreateLdrRef;

    if (_mmapOptions.wipeHeader)
        flags |= blackbone::WipeHeader;

    if (_mmapOptions.noTls)
        flags |= blackbone::NoTLS;

    if (_mmapOptions.noExceptions)
        flags |= blackbone::NoExceptions;

    if (_mmapOptions.hideVad)
        flags |= blackbone::HideVAD;

    return flags;
}

/// <summary>
/// Update interface manual map flags
/// </summary>
/// <param name="flags">Flags</param>
/// <returns>Flags</returns>
DWORD MainDlg::MmapFlags( blackbone::eLoadFlags flags )
{
    _mmapOptions.manualInmport.checked( flags & blackbone::ManualImports ? true : false );
    _mmapOptions.addLdrRef.checked( flags & blackbone::CreateLdrRef ? true : false );
    _mmapOptions.wipeHeader.checked( flags & blackbone::WipeHeader ? true : false );
    _mmapOptions.noTls.checked( flags & blackbone::NoTLS ? true : false );
    _mmapOptions.noExceptions.checked( flags & blackbone::NoExceptions ? true : false );
    _mmapOptions.hideVad.checked( flags & blackbone::HideVAD ? true : false );

    return flags;
}


/// <summary>
/// Set injection method
/// </summary>
/// <param name="mode">Injection mode</param>
/// <returns>Error code</returns>
DWORD MainDlg::SetMapMode( MapMode mode )
{
    // Reset everything
    _procCmdLine.enable();
    _procList.enable();
    _threadList.enable();
    _unlink.enable();
    _initFuncList.enable();
    _initArg.enable();

    _mmapOptions.manualInmport.enable();
    _mmapOptions.addLdrRef.enable();
    _mmapOptions.wipeHeader.enable();
    _mmapOptions.noTls.enable();
    _mmapOptions.noExceptions.enable();
    if (blackbone::Driver().loaded())
        _mmapOptions.hideVad.enable();

    switch (mode)
    {
        case Normal:
            _mmapOptions.manualInmport.disable();
            _mmapOptions.addLdrRef.disable();
            _mmapOptions.wipeHeader.disable();
            _mmapOptions.noTls.disable();
            _mmapOptions.noExceptions.disable();
            _mmapOptions.hideVad.disable();
            break;

        case Manual:
            _threadList.selection( 0 );
            _threadList.disable();
            _unlink.disable();
            break;

        case Kernel_Thread:
        case Kernel_APC:
        case Kernel_DriverMap:
            _threadList.selection( 0 );
            _threadList.disable();
            _unlink.disable();

            _initFuncList.selectedText( L"" );
            _initFuncList.disable();
            _initArg.reset();
            _initArg.disable();

            _mmapOptions.manualInmport.disable();
            _mmapOptions.addLdrRef.disable();
            _mmapOptions.wipeHeader.disable();
            _mmapOptions.noTls.disable();
            _mmapOptions.noExceptions.disable();
            _mmapOptions.hideVad.disable();

            if (mode == Kernel_DriverMap)
            {
                _procCmdLine.disable();
                _procList.disable();
            }

            break;

        default:
            break;
    }

    return ERROR_SUCCESS;
}