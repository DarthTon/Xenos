#include "DlgModules.h"
#include "Message.hpp"


ModulesDlg::ModulesDlg( blackbone::Process& proc )
    : Dialog( IDD_MODULES )
    , _process( proc )
{
    _messages[WM_INITDIALOG]   = static_cast<Dialog::fnDlgProc>(&ModulesDlg::OnInit);

    _events[IDC_BUTTON_CLOSE]  = static_cast<Dialog::fnDlgProc>(&ModulesDlg::OnCloseBtn);
    _events[IDC_BUTTON_UNLOAD] = static_cast<Dialog::fnDlgProc>(&ModulesDlg::OnUnload);
}

ModulesDlg::~ModulesDlg()
{
}

INT_PTR ModulesDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    Dialog::OnInit( hDlg, message, wParam, lParam );

    LVCOLUMNW lvc = { 0 };
    _modList.Attach( GetDlgItem( hDlg, IDC_LIST_MODULES ) );

    ListView_SetExtendedListViewStyle( _modList.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );

    //
    // Insert columns
    //
    _modList.AddColumn( L"Name",       100, Name );
    _modList.AddColumn( L"Image Base", 100, ImageBase );
    _modList.AddColumn( L"Platform",   60,  Platform );
    _modList.AddColumn( L"Load type",  80,  LoadType );

    RefrestList();

    return TRUE;
}


INT_PTR ModulesDlg::OnCloseBtn( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    EndDialog( hDlg, 0 );
    return TRUE;
}

INT_PTR ModulesDlg::OnUnload( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    // Get selection
    auto idx = _modList.selection();
    if (idx == MAXUINT)
        idx = 0;

    if (_process.valid())
    {
        wchar_t* pEnd = nullptr;
        blackbone::module_t modBase = wcstoull( _modList.itemText( idx, ImageBase ).c_str(), &pEnd, 0x10 );
        auto mod = _process.modules().GetModule( modBase );
        auto barrier = _process.core().native()->GetWow64Barrier();

        // Validate module
        if (barrier.type == blackbone::wow_32_32 && mod->type == blackbone::mt_mod64)
        {
            Message::ShowError( hDlg, L"Please use Xenos64.exe to unload 64 bit modules from WOW64 process" );
            return TRUE;
        }

        if (mod != nullptr)
        {
            _process.modules().Unload( mod );
            RefrestList();
        }
        else
            Message::ShowError( hDlg, L"Module not found" );
    }

    return TRUE;
}


void ModulesDlg::RefrestList( )
{
    _modList.reset();
    if (!_process.valid())
        return;

    // Found modules
    auto modsLdr = _process.modules().GetAllModules( blackbone::LdrList );
    auto modsPE = _process.modules().GetAllModules( blackbone::PEHeaders );

    // Known manual modules
    decltype(modsLdr) modsManual;
    _process.modules().GetManualModules( modsManual );

    for (auto& mod : modsPE)
    {
        // Avoid manual modules duplication
        if (mod.second.name.find( L"Unknown_0x" ) == 0)
        {
            auto iter = std::find_if( modsManual.begin(), modsManual.end(), [&mod]( const decltype(modsLdr)::value_type& val )
                                      { return val.second.baseAddress == mod.second.baseAddress; } );

            if (iter != modsManual.end())
                continue;
        }

        wchar_t address[64];
        wchar_t* platfom = nullptr;
        wchar_t* detected = nullptr;

        wsprintf( address, L"0x%08I64x", mod.second.baseAddress );

        // Module platform
        if (mod.second.type == blackbone::mt_mod32)
            platfom = L"32 bit";
        else if (mod.second.type == blackbone::mt_mod64)
            platfom = L"64 bit";
        else
            platfom = L"Unknown";

        // Mapping type
        if (mod.second.manual == true)
            detected = L"Manual map";
        else if (modsLdr.find( mod.first ) != modsLdr.end())
            detected = L"Normal";
        else if (mod.second.name.find( L"Unknown_0x" ) == 0)
            detected = L"PE header";
        else
            detected = L"Section only";

        _modList.AddItem( mod.second.name, static_cast<LPARAM>(mod.second.baseAddress), { address, platfom, detected } );
    }
}