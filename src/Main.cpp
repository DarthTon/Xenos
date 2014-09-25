#include "stdafx.h"
#include "MainWnd.h"
#include "DumpHandler.h"

int DumpNotifier( const wchar_t* path, void* context, EXCEPTION_POINTERS* expt, bool success )
{
    Message::ShowError( NULL, L"Program has crashed. Dump file saved at '" + std::wstring( path ) + L"'" );
    return 0;
}

int APIENTRY wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    // Setup dump generation
    dump::DumpHandler::Instance().CreateWatchdog( L".", dump::CreateFullDump, &DumpNotifier );

    MainDlg mainDlg;
    return (int)mainDlg.RunModal();
}