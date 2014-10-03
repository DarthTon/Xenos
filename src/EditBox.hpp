#pragma once

#include "Control.hpp"

namespace ctrl
{

class EditBox : public Control
{
public:
    EditBox( HWND hwnd = NULL )
        : Control( hwnd )  {  }

    virtual std::wstring text() const 
    {
        wchar_t buf[512] = { 0 };
        Edit_GetText( _hwnd, buf, ARRAYSIZE( buf ) );

        return buf;
    }

    virtual inline long integer()  { return std::wcstol( text().c_str(), nullptr, 10 ); }

    virtual inline BOOL text( const std::wstring& text ) const { return Edit_SetText( _hwnd, text.c_str() ); }

    virtual inline void reset() { Edit_SetText( _hwnd, L"" ); }
};

}