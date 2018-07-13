#pragma once

#include "resource.h"
#include <BlackBone/src/3rd_Party/VersionApi.h>
#include <BlackBone/src/BlackBone/DriverControl/DriverControl.h>

class DriverExtract
{
public:
    static DriverExtract& Instance()
    {
        static DriverExtract inst;
        return inst;
    }

    /// <summary>
    /// Extracts required driver version form self
    /// </summary>
    void Extract()
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
            HANDLE hFile = CreateFileW(
                (blackbone::Utils::GetExeDirectory() + L"\\" + filename).c_str(),
                FILE_GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL
                );

            if (hFile != INVALID_HANDLE_VALUE)
            {
                DWORD bytes = 0;
                WriteFile( hFile, pDriverData, SizeofResource( NULL, resInfo ), &bytes, NULL );
                CloseHandle( hFile );
            }
        }
    }


    ~DriverExtract()
    {
        Cleanup();
    }

    /// <summary>
    /// Remove unpacked driver, if any
    /// </summary>
    void Cleanup()
    {
        const wchar_t* filename = L"BlackBoneDrv7.sys";

        if (IsWindows10OrGreater())
            filename = L"BlackBoneDrv10.sys";
        else if (IsWindows8Point1OrGreater())
            filename = L"BlackBoneDrv81.sys";
        else if (IsWindows8OrGreater())
            filename = L"BlackBoneDrv8.sys";

        DeleteFileW( (blackbone::Utils::GetExeDirectory() + L"\\" + filename).c_str() );
    }

private:
    DriverExtract() = default;
    DriverExtract( const DriverExtract& ) = default;
};
