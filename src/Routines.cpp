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
            blackbone::pe::listExports exports;

            if (_core.LoadImageFile( cfg.imagePath, exports ) == ERROR_SUCCESS)
            {
                _imagePath.text( cfg.imagePath );

                _initFuncList.reset();
                for (auto& exprt : exports)
                    _initFuncList.Add( exprt.first );
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

        // Process mode
        switch (cfg.processMode)
        {
            case Existing:
                _exProc.checked( true );
                break;

            case NewProcess:
                _newProc.checked( true );
                break;

            case ManualLaunch:
                _autoProc.checked( true );
                break;
        }

        // Injection type
        _injectionType.selection( cfg.injectMode );

        UpdateInterface( (MapMode)cfg.injectMode );
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
    cfg.processMode    = (_exProc ? Existing : (_newProc ? NewProcess : ManualLaunch));
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
    _procList.reset();

    std::vector<blackbone::ProcessInfo> found;
    blackbone::Process::EnumByNameOrPID( 0, L"", found );

    for (auto& proc : found)
    {
        wchar_t text[255] = { 0 };
        swprintf_s( text, L"%ls (%d)", proc.imageName.c_str(), proc.pid );

        _procList.Add( text, proc.pid );
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Retrieve process threads
/// </summary>
/// <param name="pid">Process ID</param>
/// <returns>Error code</returns>
DWORD MainDlg::FillThreads( DWORD pid )
{
    _threadList.reset();
    _threadList.Add( L"New Thread" );

    std::vector<blackbone::ProcessInfo> found;
    blackbone::Process::EnumByNameOrPID( pid, L"", found, true );
    if (found.empty())
        return ERROR_NOT_FOUND;

    for (auto& thd : found.front().threads)
        _threadList.Add( std::to_wstring( thd.tid ) + (thd.mainThread ? L" (Main)" : L""), thd.tid );

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
        _procList.Add( blackbone::Utils::StripPath( path ), 0 );
        _procList.selection( 0 );

        // Enable command line options field
        _procCmdLine.enable();
        _threadList.reset();
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

    if (_mmapOptions.hideVad && blackbone::Driver().loaded())
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
    // Exclude HideVAD if no driver present
    if (!blackbone::Driver().loaded())
        flags &= ~blackbone::HideVAD;

    _mmapOptions.manualInmport.checked( flags & blackbone::ManualImports ? true : false );
    _mmapOptions.addLdrRef.checked( flags & blackbone::CreateLdrRef ? true : false );
    _mmapOptions.wipeHeader.checked( flags & blackbone::WipeHeader ? true : false );
    _mmapOptions.noTls.checked( flags & blackbone::NoTLS ? true : false );
    _mmapOptions.noExceptions.checked( flags & blackbone::NoExceptions ? true : false );
    _mmapOptions.hideVad.checked( flags & blackbone::HideVAD ? true : false );

    return flags;
}


/// <summary>
/// Update interface controls
/// </summary>
/// <param name="mode">Injection mode</param>
/// <returns>Error code</returns>
DWORD MainDlg::UpdateInterface( MapMode mode )
{
    // Reset controls state
    if (_exProc.checked())
    {
        _procCmdLine.disable();
        _procList.enable();
        _threadList.enable();
        _selectProc.disable();
    }
    else
    {
        _autoProc.checked() ? _procCmdLine.disable() : _procCmdLine.enable();
        _procList.disable();
        _threadList.disable();
        _selectProc.enable();
    }

    _exProc.enable();
    _newProc.enable();
    _autoProc.enable();
    _initFuncList.enable();
    _initArg.enable();
    _unlink.enable();

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
            _mmapOptions.manualInmport.enable();
            _mmapOptions.addLdrRef.enable();
            _mmapOptions.wipeHeader.enable();
            _mmapOptions.noTls.enable();
            _mmapOptions.noExceptions.enable();
            if (blackbone::Driver().loaded())
                _mmapOptions.hideVad.enable();

            _threadList.selection( 0 );
            _threadList.disable();
            _unlink.disable();
            break;

        case Kernel_Thread:
        case Kernel_APC:
        case Kernel_DriverMap:
            _threadList.selection( 0 );
            _threadList.disable();

            _mmapOptions.manualInmport.disable();
            _mmapOptions.addLdrRef.disable();
            _mmapOptions.wipeHeader.disable();
            _mmapOptions.noTls.disable();
            _mmapOptions.noExceptions.disable();
            _mmapOptions.hideVad.disable();

            if (mode == Kernel_DriverMap)
            {
                _unlink.disable();
                _initFuncList.selectedText( L"" );
                _initFuncList.disable();
                _initArg.reset();
                _initArg.disable();
                _exProc.disable();
                _newProc.disable();
                _autoProc.disable();
                _procCmdLine.disable();
                _procList.disable();
                _selectProc.disable();
            }

            break;

        default:
            break;
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Select executable image via file selection dialog
/// </summary>
/// <param name="selectedPath">Selected path</param>
/// <returns>true if image was selected, false if canceled</returns>
bool MainDlg::SelectExecutable( std::wstring& selectedPath )
{
    OPENFILENAMEW ofn = { 0 };
    wchar_t path[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = TEXT( "All (*.*)\0*.*\0Executable image (*.exe)\0*.exe\0" );
    ofn.nFilterIndex = 2;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST;

    if (GetOpenFileName( &ofn ))
    {
        selectedPath = path;
        return true;
    }

    return false;
}