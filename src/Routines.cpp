#include "MainDlg.h"
#include "DriverExtract.h"

/// <summary>
/// Load configuration from file
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::LoadConfig( const std::wstring& path /*= L""*/ )
{
    auto& cfg = _profileMgr.config();
    if (_profileMgr.Load( path ))
    {
        // Image
        for (auto& path : cfg.images)
            LoadImageFile( path );

        // Process
        if (!cfg.procName.empty())
        {
            if (cfg.processMode != Existing)
            {
                SetActiveProcess( 0, cfg.procName );
            }
            else
            {
                std::vector<DWORD> pidList = blackbone::Process::EnumByName( cfg.procName );
                if (!pidList.empty())
                {
                    cfg.pid = pidList.front();

                    auto idx = _procList.Add( cfg.procName + L" (" + std::to_wstring( pidList.front() ) + L")", pidList.front() );
                    _procList.selection( idx );

                    SetActiveProcess( pidList.front(), cfg.procName );
                }
            }
        }

        // Process mode
        _exProc.checked( false );
        _newProc.checked( false );
        _autoProc.checked( false );
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

        // Fix injection method
        if (cfg.injectMode >= Kernel_Thread)
        {
            DriverExtract::Instance().Extract();
            if (!NT_SUCCESS( blackbone::Driver().EnsureLoaded() ))
            {
                DriverExtract::Instance().Cleanup();
                cfg.injectMode = Normal;
            }
        }

        // Update profile name
        if (!_defConfig.empty())
            _status.SetText( 0, blackbone::Utils::StripPath( _defConfig ) );
    }
    // Default settings
    else
    {
        _exProc.checked( true );
    }

    UpdateInterface();
    return ERROR_SUCCESS;
}


/// <summary>
/// Save Configuration.
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::SaveConfig( const std::wstring& path /*= L""*/ )
{
    auto& cfg = _profileMgr.config();

    cfg.procName = _processPath;
    cfg.images.clear();
    for (auto& img : _images)
        cfg.images.emplace_back( img->path() );

    cfg.processMode = (_exProc ? Existing : (_newProc ? NewProcess : ManualLaunch));

    _profileMgr.Save( path );
    return ERROR_SUCCESS;
}

/// <summary>
/// Enumerate processes
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::FillProcessList()
{
    _procList.reset();

    auto found = blackbone::Process::EnumByNameOrPID( 0, L"" ).result( std::vector<blackbone::ProcessInfo>() );
    for (auto& proc : found)
    {
        wchar_t text[255] = { 0 };
        swprintf_s( text, L"%ls (%d)", proc.imageName.c_str(), proc.pid );

        _procList.Add( text, proc.pid );
    }

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
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Async inject routine
/// </summary>
void MainDlg::Inject()
{
    _inject.disable();

    InjectContext context;
    auto& cfg = _profileMgr.config();
    NTSTATUS status = STATUS_SUCCESS;

    // Fill in context
    context.cfg = cfg;
    context.images = _images;
    context.pid = _procList.itemData( _procList.selection() );
    context.procPath = _processPath;
    context.skippedCount = 0;
    context.procList.clear();
    context.procDiff.clear();

    _status.SetText( 2, L"Injecting..." );

    // Show wait dialog
    if (context.cfg.processMode == ManualLaunch)
    {
        DlgWait dlgWait( _core, context );
        dlgWait.RunModal( _hwnd );
        status = dlgWait.status();
    }
    else
    {
        status = _core.InjectMultiple( &context );
    }

    // Close after successful injection
    if (cfg.close && NT_SUCCESS( status ))
    {
        SaveConfig();
        CloseDialog();
        return;
    }

    _inject.enable();
    _status.SetText( 2, L"Idle" );
}

/// <summary>
/// Update interface controls
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::UpdateInterface( )
{
    // Reset controls state
    if (_exProc.checked())
    {
        _procList.enable();
        _selectProc.disable();
    }
    else
    {
        _procList.disable();
        _selectProc.enable();
    }

    if (_profileMgr.config().injectMode == Kernel_DriverMap)
    {
        _exProc.disable();
        _newProc.disable();
        _autoProc.disable();
        _procList.disable();
        _selectProc.disable();
    }
    else
    {
        _exProc.enable();
        _newProc.enable();
        _autoProc.enable();
    }

    // Disable inject button if no images available or no process selected
    if (_images.empty() || _procList.selection() == -1)
        _inject.disable();
    else
        _inject.enable();

    return ERROR_SUCCESS;
}

/// <summary>
/// Invoke Open/Save file dialog
/// </summary>
/// <param name="filter">File filter</param>
/// <param name="defIndex">Default filter index</param>
/// <param name="selectedPath">Target file path</param>
/// <param name="bSave">true to Save file, false to open</param>
/// <param name="defExt">Default file extension for file save</param>
/// <returns>true on success, false if operation was canceled by user</returns>
bool MainDlg::OpenSaveDialog( 
    const wchar_t* filter, 
    int defIndex, 
    std::wstring& selectedPath,
    bool bSave /*= false*/, 
    const std::wstring& defExt /*= L""*/
    )
{
    OPENFILENAMEW ofn = { 0 };
    wchar_t path[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = defIndex;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    if (!bSave)
    {
        ofn.Flags = OFN_PATHMUSTEXIST;
    }
    else
    {
        ofn.Flags = OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = defExt.c_str();
    }

    auto res = bSave ? GetSaveFileNameW( &ofn ) : GetOpenFileNameW( &ofn );
    if (res)
        selectedPath = path;

    return res != FALSE;
}

/// <summary>
/// Load selected image and do some validation
/// </summary>
/// <param name="path">Full qualified image path</param>
/// <returns>Error code </returns>
DWORD MainDlg::LoadImageFile( const std::wstring& path )
{
    std::shared_ptr<blackbone::pe::PEImage> img( new blackbone::pe::PEImage );
    blackbone::pe::vecExports exports;

    // Check if image is already in the list
    if (std::find_if( _images.begin(), _images.end(),
        [&path]( std::shared_ptr<blackbone::pe::PEImage>& img ) { return path == img->path(); } ) != _images.end())
    {
        Message::ShowInfo( _hwnd, L"Image '" + path + L"' is already in the list" );
        return ERROR_ALREADY_EXISTS;
    }

    // Check if image is a PE file
    if (!NT_SUCCESS( img->Load( path ) ))
    {
        std::wstring errstr = std::wstring( L"File \"" ) + path + L"\" is not a valid PE image";
        Message::ShowError( _hwnd, errstr.c_str() );

        img->Release();
        return ERROR_INVALID_IMAGE_HASH;
    }

    // In case of pure IL, list all methods
    if (img->pureIL() && img->net().Init( path ))
    {
        blackbone::ImageNET::mapMethodRVA methods;
        img->net().Parse( &methods );

        for (auto& entry : methods)
        {
            std::wstring name = entry.first.first + L"." + entry.first.second;
            exports.push_back( blackbone::pe::ExportData( blackbone::Utils::WstringToAnsi( name ), 0 ) );
        }
    }
    // Simple exports otherwise
    else
        img->GetExports( exports );

    
    // Add to internal lists
    AddToModuleList( img );
    _exports.emplace_back( exports );

    if (_procList.selection() != -1)
        _inject.enable();

    img->Release( true );
    return ERROR_SUCCESS;
}

/// <summary>
/// Add module to module list
/// </summary>
/// <param name="path">Loaded image</param>
/// <param name="exports">Module exports</param>
void MainDlg::AddToModuleList( std::shared_ptr<blackbone::pe::PEImage>& img )
{
    wchar_t* platfom = nullptr;

    // Module platform
    if (img->mType() == blackbone::mt_mod32)
        platfom = L"32 bit";
    else if (img->mType() == blackbone::mt_mod64)
        platfom = L"64 bit";
    else
        platfom = L"Unknown";

    _images.emplace_back( img );
    _modules.AddItem( blackbone::Utils::StripPath( img->path() ), 0, { platfom } );
}
