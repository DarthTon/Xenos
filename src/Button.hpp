#pragma once

#include "Control.hpp"

namespace ctrl
{

class Button : public Control
{
public:
    Button( HWND hwnd = NULL )
        : Control( hwnd )  {  }

    virtual bool checked() const        { return Button_GetCheck( _hwnd ) != BST_UNCHECKED; }
    virtual void checked( bool state )  { Button_SetCheck( _hwnd, state ); }

    inline operator bool() { return checked(); }
};

}