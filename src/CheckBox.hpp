#pragma once

#include "Control.hpp"

namespace ctrl
{

class CheckBox : public Control
{
public:
    CheckBox( HWND hwnd = NULL )
        : Control( hwnd )  {  }

    virtual bool checked() const        { return Button_GetCheck( _hwnd ) != 0; }
    virtual void checked( bool state )  { Button_SetCheck( _hwnd, state ); }

    inline operator bool() { return checked(); }
};

}