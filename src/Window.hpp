#pragma once

#include "stdafx.h"
#include <string>

#define MSG_HANDLER(n) virtual INT_PTR n(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

class Window
{
public:
    Window( HWND hwnd = NULL )
        : _hwnd( hwnd )  {  }

    virtual inline void Attach( HWND hwnd ) { _hwnd = hwnd; }
    virtual inline void Attach( HWND hDlg, UINT id ) { _hwnd = GetDlgItem( hDlg, id ); }

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