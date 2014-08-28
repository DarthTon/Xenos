#pragma once

#include "Control.hpp"

namespace ctrl
{

class ComboBox : public Control
{
public:
    ComboBox( HWND hwnd = NULL )
        : Control( hwnd )  {  }

    virtual inline int selection() const      { return ComboBox_GetCurSel( _hwnd ); }
    virtual inline int selection( int index ) { return ComboBox_SetCurSel( _hwnd, index ); }

    virtual inline int itemData( int index ) const     { return (int)ComboBox_GetItemData( _hwnd, index ); }
    virtual inline int itemData( int index, int data ) { return (int)ComboBox_SetItemData( _hwnd, index, data ); }

    virtual inline void reset() { ComboBox_ResetContent( _hwnd ); }

    virtual int Add( const std::wstring& text, int data = 0 )
    { 
        auto idx = ComboBox_AddString( _hwnd, text.c_str() ); 
        ComboBox_SetItemData( _hwnd, idx, data );

        return idx;
    }

    virtual int Add( const std::string& text, int data = 0 )
    {
        auto idx = (int)SendMessageA( _hwnd, CB_ADDSTRING, 0, (LPARAM)text.c_str() );
        ComboBox_SetItemData( _hwnd, idx, data );

        return idx;
    }

    virtual std::wstring itemText( int index ) const
    {
        wchar_t buf[512] = { 0 };
        ComboBox_GetLBText( _hwnd, index, buf );

        return buf;
    }

    virtual void modifyItem( int index, const std::wstring& text, int data = 0 )
    {
        auto oldData = itemData( index );
        if (data == 0)
            data = oldData;

        ComboBox_DeleteString( _hwnd, index );
        index = ComboBox_InsertString( _hwnd, index, text.c_str() );
        ComboBox_SetItemData( _hwnd, index, data );
    }

    virtual std::wstring selectedText() const
    {
        wchar_t buf[512] = { 0 };
        ComboBox_GetText( _hwnd, buf, ARRAYSIZE(buf) );

        return buf;
    }

    virtual inline void selectedText( const std::wstring& text ) { ComboBox_SetText( _hwnd, text.c_str() ); }
};

}