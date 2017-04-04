#pragma once

#include "stdafx.h"
#include <BlackBone/src/BlackBone/Misc/Thunk.hpp>
#include <string>
#include <map>

#define MSG_HANDLER( n ) virtual INT_PTR n( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )

class Window
{
public:
    typedef INT_PTR( Window::*fnWndProc )(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    typedef std::map<UINT, std::pair<Window*, fnWndProc>> mapWndProc;

public:
    Window( HWND hwnd = NULL )
        : _hwnd( hwnd )
        , _subThunk( &Window::SubProc, this ) { }
 
    virtual inline void Attach( HWND hwnd ) { _hwnd = hwnd; }
    virtual inline void Attach( HWND hDlg, UINT id ) { _id = id; _hwnd = GetDlgItem( hDlg, id ); }

    virtual inline HWND hwnd() const { return _hwnd; }
    virtual inline UINT id()   const { return _id; }

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

    virtual inline WNDPROC oldProc() const { return _oldProc; }

    virtual void Subclass( UINT message, fnWndProc handler, Window* instance = nullptr )
    {
        // Remove old handler
        if(handler == nullptr)
        {
            if(_subMessages.count(message))
            {
                _subMessages.erase( message );

                // Remove subproc
                if(_subMessages.empty())
                {
                    SetWindowLongPtrW( _hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_oldProc) );
                    _oldProc = nullptr;
                }

                return;
            }
        }
        else
        {
            _subMessages[message] = std::make_pair( instance ? instance : this, handler );
            if (!_oldProc)
                _oldProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW( _hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_subThunk.GetThunk()) ));
        }       
    }

private:
    LRESULT SubProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
    {
        if (_subMessages.count( message ))
            return (this->_subMessages[message].first->*_subMessages[message].second)(hwnd, message, wParam, lParam);

        return CallWindowProcW( _oldProc, hwnd, message, wParam, lParam );
    }

protected:
    HWND _hwnd = NULL;
    UINT _id = 0;
    WNDPROC _oldProc = nullptr;
    mapWndProc _subMessages;
    Win32Thunk<WNDPROC, Window> _subThunk;
};