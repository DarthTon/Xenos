#include "DlgSettings.h"
#include "DriverExtract.h"
#include "resource.h"

DlgSettings::DlgSettings( ProfileMgr& cfgMgr )
    : Dialog( IDD_SETTINGS )
    , _profileMgr( cfgMgr )
{
    _events[CBN_SELCHANGE]      = static_cast<Dialog::fnDlgProc>(&DlgSettings::OnSelChange);
    _events[ID_SETTINGS_OK]     = static_cast<Dialog::fnDlgProc>(&DlgSettings::OnOK);
    _events[ID_SETTINGS_CANCEL] = static_cast<Dialog::fnDlgProc>(&DlgSettings::OnCancel);
}

DlgSettings::~DlgSettings()
{
}


INT_PTR DlgSettings::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    Dialog::OnInit( hDlg, message, wParam, lParam );

    _hijack.Attach( _hwnd, IDC_HIJACK );
    _injectionType.Attach( _hwnd, IDC_OP_TYPE );
    _initFuncList.Attach( _hwnd, IDC_INIT_EXPORT );

    _procCmdLine.Attach( _hwnd, IDC_CMDLINE );
    _initArg.Attach( _hwnd, IDC_ARGUMENT );
    _injClose.Attach( _hwnd, IDC_INJ_CLOSE );
    _unlink.Attach( _hwnd, IDC_UNLINK );
    _erasePE.Attach( _hwnd, IDC_WIPE_HDR_NATIVE );
    _krnHandle.Attach( _hwnd, IDC_KRN_HANDLE );
    _injIndef.Attach( _hwnd, IDC_INJ_EACH );

    _delay.Attach( _hwnd, IDC_DELAY );
    _period.Attach( _hwnd, IDC_PERIOD );

    _skipProc.Attach( _hwnd, IDC_SKIP );

    _mmapOptions.addLdrRef.Attach( GetDlgItem( _hwnd, IDC_LDR_REF ) );
    _mmapOptions.manualInmport.Attach( GetDlgItem( _hwnd, IDC_MANUAL_IMP ) );
    _mmapOptions.noTls.Attach( GetDlgItem( _hwnd, IDC_IGNORE_TLS ) );
    _mmapOptions.noExceptions.Attach( GetDlgItem( _hwnd, IDC_NOEXCEPT ) );
    _mmapOptions.wipeHeader.Attach( GetDlgItem( _hwnd, IDC_WIPE_HDR ) );
    _mmapOptions.hideVad.Attach( GetDlgItem( _hwnd, IDC_HIDEVAD ) );

    _mmapOptions.hideVad.disable();

    // Fill injection types
    _injectionType.Add( L"Native inject", Normal );
    _injectionType.Add( L"Manual map", Manual );

    _injectionType.Add( L"Kernel (CreateThread)", Kernel_Thread );
    _injectionType.Add( L"Kernel (APC)", Kernel_APC );
    _injectionType.Add( L"Kernel (Manual map)", Kernel_MMap );
    //_injectionType.Add( L"Kernel driver manual map", Kernel_DriverMap );

    // Disable some driver-dependent stuff
    if (blackbone::Driver().loaded())
        _mmapOptions.hideVad.enable();

    _injectionType.selection( 0 );

    // List exports
    _initFuncList.reset();
    for (auto& exp : _exports)
        _initFuncList.Add( exp.name );

    // Setup interface from current config
    UpdateFromConfig();
    UpdateInterface();

    return TRUE;
}

INT_PTR DlgSettings::OnOK( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    // Update config
    SaveConfig();
    return Dialog::OnClose( hDlg, message, wParam, lParam );
}

INT_PTR DlgSettings::OnCancel( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    return Dialog::OnClose( hDlg, message, wParam, lParam );
}

INT_PTR DlgSettings::OnSelChange( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    // Injection type selection
    if (LOWORD( wParam ) == IDC_OP_TYPE)
    {
        if (HandleDriver( _injectionType.selection() ) == ERROR_SUCCESS)
        {
            _lastSelected = _profileMgr.config().injectMode = _injectionType.selection();
            UpdateInterface();
        }
    }

    return TRUE;
}

/// <summary>
/// Handle BlackBone driver load problems
/// </summary>
/// <param name="type">Injection type</param>
/// <returns>Error code</returns>
DWORD DlgSettings::HandleDriver( uint32_t type )
{
    if (type < Kernel_Thread)
        return STATUS_SUCCESS;

    DriverExtract::Instance().Extract();
    auto status = blackbone::Driver().EnsureLoaded();

    // Try to enable test signing
    if (!NT_SUCCESS( status ))
    {
        auto text = L"Failed to load BlackBone driver:\n\n" + blackbone::Utils::GetErrorDescription( status );
        Message::ShowError( _hwnd, text );

        // Detect test signing
        if (status == STATUS_INVALID_IMAGE_HASH)
        {
            // Ask user to enable test signing
            if (Message::ShowQuestion( _hwnd, L"Would you like to enable Driver Test signing mode to load driver?" ))
            {
                STARTUPINFOW si = { 0 };
                PROCESS_INFORMATION pi = { 0 };
                wchar_t windir[255] = { 0 };
                PVOID oldVal = nullptr;

                GetWindowsDirectoryW( windir, sizeof( windir ) );
                std::wstring bcdpath = std::wstring( windir ) + L"\\system32\\cmd.exe";

                Wow64DisableWow64FsRedirection( &oldVal );

                // For some reason running BCDedit directly does not enable test signing from WOW64 process
                if (CreateProcessW( bcdpath.c_str(), L"/C Bcdedit.exe -set TESTSIGNING ON", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
                {
                    Message::ShowWarning( _hwnd, L"You must reboot your computer for the changes to take effect" );

                    CloseHandle( pi.hProcess );
                    CloseHandle( pi.hThread );
                }
                else
                {
                    Message::ShowError( _hwnd, L"Failed to enable Test signing. Please do it manually" );
                    status = LastNtStatus();
                }

                Wow64RevertWow64FsRedirection( oldVal );
            }
        }

        // Revert selection
        DriverExtract::Instance().Cleanup();
        _injectionType.selection( _lastSelected );
    }

    return status;
}

/// <summary>
/// Update interface accordingly to current config
/// </summary>
/// <returns>Error code</returns>
DWORD DlgSettings::UpdateFromConfig()
{
    auto& cfg = _profileMgr.config();

    // Options
    _procCmdLine.text( cfg.procCmdLine );
    _initArg.text( cfg.initArgs );
    _initFuncList.text( cfg.initRoutine );

    _delay.text( std::to_wstring( cfg.delay ) );
    _period.text( std::to_wstring( cfg.period ) );
    _skipProc.text( std::to_wstring( cfg.skipProc ) );

    _unlink.checked( cfg.unlink );
    _erasePE.checked( cfg.erasePE );
    _injClose.checked( cfg.close );
    _injIndef.checked( cfg.injIndef );

    _lastSelected = cfg.injectMode;
    _injectionType.selection( cfg.injectMode );
    _hijack.checked( cfg.hijack );

    if (blackbone::Driver().loaded())
        _krnHandle.checked( cfg.krnHandle );

    MmapFlags( (blackbone::eLoadFlags)cfg.mmapFlags );

    return ERROR_SUCCESS;
}

/// <summary>
/// Save Configuration.
/// </summary>
/// <returns>Error code</returns>
DWORD DlgSettings::SaveConfig()
{
    auto& cfg = _profileMgr.config();

    cfg.procCmdLine    = _procCmdLine.text();
    cfg.initRoutine    = _initFuncList.selectedText();
    cfg.initArgs       = _initArg.text();
    cfg.injectMode     = _injectionType.selection();
    cfg.mmapFlags      = MmapFlags();
    cfg.unlink         = _unlink.checked();
    cfg.erasePE        = _erasePE.checked();
    cfg.close          = _injClose.checked();
    cfg.hijack         = _hijack.checked();
    cfg.krnHandle      = _krnHandle.checked();
    cfg.delay          = _delay.integer();
    cfg.period         = _period.integer();
    cfg.skipProc       = _skipProc.integer();
    cfg.injIndef       = _injIndef.checked();

    _profileMgr.Save();
    return ERROR_SUCCESS;
}

/// <summary>
/// Update interface controls
/// </summary>
void DlgSettings::UpdateInterface()
{
    // Reset controls state
    auto& cfg = _profileMgr.config();

    _hijack.enable();
    _initFuncList.enable();
    _initArg.enable();
    _erasePE.enable();
    _unlink.enable();

    if (blackbone::Driver().loaded())
        _krnHandle.enable();

    switch (cfg.processMode)
    {
    case NewProcess:
        _procCmdLine.enable();
        _skipProc.disable();
        _injIndef.disable();
        break;
    case Existing:
        _procCmdLine.disable();
        _skipProc.disable();
        _injIndef.disable();
        break;
    case ManualLaunch:
        _procCmdLine.disable();
        _skipProc.enable();
        _injIndef.enable();
        break;
    };


    switch (cfg.injectMode)
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

            _hijack.disable();
            _unlink.disable();
            _erasePE.disable();
            break;

        case Kernel_Thread:
        case Kernel_APC:
        case Kernel_MMap:
        case Kernel_DriverMap:        
            _hijack.disable();
            _krnHandle.disable();

            _mmapOptions.addLdrRef.disable();

            if (cfg.injectMode == Kernel_MMap)
            {
                _mmapOptions.addLdrRef.checked( false );
                _mmapOptions.manualInmport.enable();
                _mmapOptions.wipeHeader.enable();
                _mmapOptions.noTls.enable();
                _mmapOptions.noExceptions.enable();
                _mmapOptions.hideVad.enable();

                _unlink.disable();
                _erasePE.disable();
            }
            else
            {
                _mmapOptions.manualInmport.disable();
                _mmapOptions.wipeHeader.disable();
                _mmapOptions.noTls.disable();
                _mmapOptions.noExceptions.disable();
                _mmapOptions.hideVad.disable();
            }

            if (cfg.injectMode == Kernel_DriverMap)
            {
                _unlink.disable();
                _erasePE.disable();
                _initFuncList.selectedText( L"" );
                _initFuncList.disable();
                _initArg.reset();
                _initArg.disable();
                _procCmdLine.disable();
            }

            break;

        default:
            break;
    }
}

/// <summary>
/// Get manual map flags from interface
/// </summary>
/// <returns>Flags</returns>
blackbone::eLoadFlags DlgSettings::MmapFlags()
{
    blackbone::eLoadFlags flags = blackbone::NoFlags;

    if (_mmapOptions.manualInmport)
        flags |= blackbone::ManualImports;

    if (_mmapOptions.addLdrRef && _injectionType.selection() != Kernel_MMap)
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
DWORD DlgSettings::MmapFlags( blackbone::eLoadFlags flags )
{
    // Exclude HideVAD if no driver present
    if (!blackbone::Driver().loaded())
        flags &= ~blackbone::HideVAD;

    // Exclude CreateLdrRef for kernel manual map
    if (_injectionType.selection() == Kernel_MMap)
        flags &= ~blackbone::CreateLdrRef;

    _mmapOptions.manualInmport.checked( flags & blackbone::ManualImports ? true : false );
    _mmapOptions.addLdrRef.checked( flags & blackbone::CreateLdrRef ? true : false );
    _mmapOptions.wipeHeader.checked( flags & blackbone::WipeHeader ? true : false );
    _mmapOptions.noTls.checked( flags & blackbone::NoTLS ? true : false );
    _mmapOptions.noExceptions.checked( flags & blackbone::NoExceptions ? true : false );
    _mmapOptions.hideVad.checked( flags & blackbone::HideVAD ? true : false );

    return flags;
}
