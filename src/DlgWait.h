#pragma once

#include "Dialog.hpp"
#include "resource.h"
#include "InjectionCore.h"

class DlgWait : public Dialog
{
public:
    DlgWait( InjectionCore& injection );
    ~DlgWait();

private:
    /// <summary>
    /// Wait for injection
    /// </summary>
    /// <returns>Error code</returns>
    DWORD WaitForInjection();

    virtual INT_PTR OnInit      ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );  
    virtual INT_PTR OnCloseBtn  ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam ); 

private:
    InjectionCore& _injection;
};

