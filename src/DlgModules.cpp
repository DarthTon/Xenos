#include "DlgModules.h"

ModulesDlg* pInstance = nullptr;

ModulesDlg::ModulesDlg( blackbone::Process& proc )
    :_process( proc )
{
    pInstance = this;

    Messages[WM_INITDIALOG] = &ModulesDlg::OnInit;
    Messages[WM_COMMAND]    = &ModulesDlg::OnCommand;
    Messages[WM_CLOSE]      = &ModulesDlg::OnClose;

    Events[IDC_BUTTON_CLOSE]    = &ModulesDlg::OnCloseBtn;
    Events[IDC_BUTTON_UNLOAD]   = &ModulesDlg::OnUnload;
}

ModulesDlg::~ModulesDlg()
{

}


INT_PTR CALLBACK ModulesDlg::DlgProcModules( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (pInstance->Messages.find( message ) != pInstance->Messages.end( ))
        return (pInstance->*pInstance->Messages[message])(hDlg, message, wParam, lParam);

    return 0;
}

INT_PTR ModulesDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    LVCOLUMNW lvc = { 0 };
    _hDlg = hDlg;

    HWND hList = GetDlgItem( hDlg, IDC_LIST_MODULES );

    ListView_SetExtendedListViewStyle( hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );

    //
    // Insert columns
    //
    lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.pszText = L"Name";
    lvc.cx = 100;
    
    ListView_InsertColumn( hList, 0, &lvc );

    lvc.pszText = L"Image Base";
    lvc.iSubItem = 1;
    lvc.cx = 100;

    ListView_InsertColumn( hList, 1, &lvc );

    lvc.pszText = L"Platform";
    lvc.iSubItem = 2;
    lvc.cx = 60;

    ListView_InsertColumn( hList, 2, &lvc );

    lvc.pszText = L"Load type";
    lvc.iSubItem = 3;
    lvc.cx = 80;

    ListView_InsertColumn( hList, 3, &lvc );

    RefrestList();

    return TRUE;
}

INT_PTR ModulesDlg::OnCommand( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (Events.count( LOWORD( wParam ) ))
        return (this->*Events[LOWORD( wParam )])(hDlg, message, wParam, lParam);

    if (Events.count( HIWORD( wParam ) ))
        return (this->*Events[HIWORD( wParam )])(hDlg, message, wParam, lParam);

    return FALSE;
}

INT_PTR ModulesDlg::OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    EndDialog( hDlg, 0 );
    return TRUE;
}

INT_PTR ModulesDlg::OnCloseBtn( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    EndDialog( hDlg, 0 );
    return TRUE;
}

INT_PTR ModulesDlg::OnUnload( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    HWND hList = GetDlgItem( _hDlg, IDC_LIST_MODULES );
    wchar_t address[64] = { 0 };

    // Get selection
    auto idx = ListView_GetNextItem( hList, 0, LVNI_SELECTED );
    if (idx == MAXUINT)
        idx = 0;

    ListView_GetItemText( hList, idx, 1, address, ARRAYSIZE( address ) );

    if (_process.valid())
    {
        wchar_t* pEnd = nullptr;
        blackbone::module_t modBase = wcstoull( address, &pEnd, 0x10 );
        auto mod = _process.modules().GetModule( modBase );
        auto barrier = _process.core().native()->GetWow64Barrier();

        // Validate module
        if (barrier.type == blackbone::wow_32_32 && mod->type == blackbone::mt_mod64)
        {
            MessageBoxW( hDlg, L"Please use Xenos64.exe to unload 64 bit modules from WOW64 process", L"Error", MB_ICONERROR );
            return TRUE;
        }

        if (mod != nullptr)
        {
            _process.modules().Unload( mod );
            RefrestList();
        }
        else
            MessageBoxW( hDlg, L"Module not found", L"Error", MB_ICONERROR );
    }

    return TRUE;
}


void ModulesDlg::RefrestList( )
{
    HWND hList = GetDlgItem( _hDlg, IDC_LIST_MODULES );
    LVITEMW lvi = { 0 };

    ListView_DeleteAllItems( hList );

    if (!_process.valid())
        return;

    lvi.mask = LVIF_TEXT | LVIF_PARAM;

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

        lvi.pszText = (LPWSTR)mod.second.name.c_str();
        lvi.cchTextMax = static_cast<int>(mod.second.name.length()) + 1;
        lvi.lParam = mod.second.baseAddress;

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

        int pos = ListView_InsertItem( hList, &lvi );
        ListView_SetItemText( hList, pos, 1, address );
        ListView_SetItemText( hList, pos, 2, platfom );
        ListView_SetItemText( hList, pos, 3, detected );
    }
}