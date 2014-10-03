#include "stdafx.h"
#include "MainDlg.h"
#include "DumpHandler.h"
#include "../../BlackBone/contrib/VersionHelpers.h"

/// <summary>
/// Crash dump notify callback
/// </summary>
/// <param name="path">Dump file path</param>
/// <param name="context">User context</param>
/// <param name="expt">Exception info</param>
/// <param name="success">if false - crash dump file was not saved</param>
/// <returns>status</returns>
int DumpNotifier( const wchar_t* path, void* context, EXCEPTION_POINTERS* expt, bool success )
{
    Message::ShowError( NULL, L"Program has crashed. Dump file saved at '" + std::wstring( path ) + L"'" );
    return 0;
}

/// <summary>
/// Associate profile file extension
/// </summary>
void AssociateExtension()
{
    wchar_t tmp[255] = { 0 };
    GetModuleFileNameW( NULL, tmp, sizeof( tmp ) );

    std::wstring alias = L"XenosProfile";
    std::wstring desc = L"Xenos injection profile";
    std::wstring openWith = std::wstring(tmp) + L" %1";
    std::wstring icon = std::wstring( tmp ) + L",-" + std::to_wstring( IDI_ICON1 );

    auto AddKey = []( const std::wstring& subkey, const std::wstring& value ){
        SHSetValue( HKEY_CLASSES_ROOT, subkey.c_str(), NULL, REG_SZ, value.c_str(), (DWORD)(value.size() * sizeof( wchar_t )) );
    };

    AddKey( L".xpr", alias );
    AddKey( alias, desc );
    AddKey( alias + L"\\shell\\open\\command", openWith );
    AddKey( alias + L"\\DefaultIcon", icon );
}


class DriverExtract
{
public:

    /// <summary>
    /// Extracts required driver version form self
    /// </summary>
    DriverExtract()
    {
        int resID = IDR_DRV7;
        const wchar_t* filename = L"BlackBoneDrv7.sys";

        if (IsWindows10OrGreater())
        {
            filename = L"BlackBoneDrv10.sys";
            resID = IDR_DRV10;
        }
        else if (IsWindows8Point1OrGreater())
        {
            filename = L"BlackBoneDrv81.sys";
            resID = IDR_DRV81;
        }
        else if (IsWindows8OrGreater())
        {
            filename = L"BlackBoneDrv8.sys";
            resID = IDR_DRV8;
        }

        HRSRC resInfo = FindResourceW( NULL, MAKEINTRESOURCEW( resID ), L"Driver" );
        if (resInfo)
        {
            HGLOBAL hRes = LoadResource( NULL, resInfo );
            PVOID pDriverData = LockResource( hRes );
            HANDLE hFile = CreateFileW( filename, FILE_GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
            if (hFile != INVALID_HANDLE_VALUE)
            {
                DWORD bytes = 0;
                WriteFile( hFile, pDriverData, SizeofResource( NULL, resInfo ), &bytes, NULL );
                CloseHandle( hFile );
            }
        }
    }

    /// <summary>
    /// Remove unpacked driver, if any
    /// </summary>
    ~DriverExtract()
    {
        const wchar_t* filename = L"BlackBoneDrv7.sys";

        if (IsWindows10OrGreater())
            filename = L"BlackBoneDrv10.sys";
        else if( IsWindows8Point1OrGreater() )
            filename = L"BlackBoneDrv81.sys";
        else if (IsWindows8OrGreater())
            filename = L"BlackBoneDrv8.sys";

        DeleteFileW( filename );
    }
};


int APIENTRY wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPWSTR lpCmdLine, int /*nCmdShow*/)
{
    DriverExtract drv;

    // Setup dump generation
    dump::DumpHandler::Instance().CreateWatchdog( blackbone::Utils::GetExeDirectory(), dump::CreateFullDump, &DumpNotifier );
    AssociateExtension();
    
    MainDlg mainDlg( lpCmdLine );
    return (int)mainDlg.RunModeless( NULL, IDR_ACCELERATOR1 );
}