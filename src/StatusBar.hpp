#pragma once

#include "Control.hpp"
#include <memory>

namespace ctrl
{

class StatusBar: public Control
{
public:
    StatusBar( HWND hwnd = NULL )
        : Control( hwnd )  {  }

    void SetParts( const std::initializer_list<int>& coords )
    {
        int i = 0;
        std::unique_ptr<int[]> coordArray( new int[coords.size()]() );
        for (auto& item : coords)
        {
            coordArray[i] = item;
            i++;
        }

        SendMessage( _hwnd, SB_SETPARTS, i, (LPARAM)coordArray.get() );
    }

    void SetText( int index, const std::wstring& text )
    {
        SendMessage( _hwnd, SB_SETTEXT, MAKEWORD( index, SBT_NOBORDERS ), (LPARAM)text.c_str() );
    }
};

}