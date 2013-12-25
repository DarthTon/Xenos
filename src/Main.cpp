#include "stdafx.h"
#include "MainWnd.h"

int APIENTRY wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    return (int)MainDlg::Instance().Run();
}