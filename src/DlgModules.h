#pragma once

#include "Dialog.hpp"
#include "resource.h"
#include "ListView.hpp"

#include "../../BlackBone/src/BlackBone/Process/Process.h"
#include "../../BlackBone/src/BlackBone/Misc/Utils.h"

class ModulesDlg : public Dialog
{
    enum ColumnID
    {
        Name = 0,
        ImageBase,
        Platform,
        LoadType
    };

public:
    ModulesDlg( blackbone::Process& proc );
    ~ModulesDlg();

private:
    void RefrestList();

    virtual INT_PTR OnInit      ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );   
    virtual INT_PTR OnCloseBtn  ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
    virtual INT_PTR OnUnload    ( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

private:
    ctrl::ListView _modList;
    blackbone::Process& _process;
};