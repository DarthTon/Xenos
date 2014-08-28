#pragma once

#include "stdafx.h"
#include <string>

namespace ctrl
{

class Control
{
public:
    Control( HWND hwnd = NULL )
        : _hwnd( hwnd )  {  }

    virtual inline void Attach( HWND hwnd ) { _hwnd = hwnd; }

    virtual inline HWND hwnd() const { return _hwnd; }

    virtual inline void enable()  const { EnableWindow( _hwnd, TRUE ); }
    virtual inline void disable() const { EnableWindow( _hwnd, FALSE ); }

    virtual std::wstring text() const
    {
        wchar_t buf[512] = { 0 };
        GetWindowTextW( _hwnd, buf, ARRAYSIZE( buf ) );

        return buf;
    }

    virtual inline BOOL text( const std::wstring& text ) 
    { 
        return SetWindowText( _hwnd, text.c_str() ); 
    }

protected:
    HWND _hwnd = NULL;
};

}