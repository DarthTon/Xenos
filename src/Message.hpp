#pragma once

#include "stdafx.h"

class Message
{
    enum MsgType
    {
        Error,
        Warning,
        Info,
    };

public:
    static void ShowError( HWND parent, const std::wstring& msg, const std::wstring& title = L"Error" )
    {
        return Show( msg, title, Error, parent );
    }

    static void ShowWarning( HWND parent, const std::wstring& msg, const std::wstring& title = L"Warning" )
    {
        return Show( msg, title, Warning, parent );
    }

    static void ShowInfo( HWND parent, const std::wstring& msg, const std::wstring& title = L"Info" )
    {
        return Show( msg, title, Info, parent );
    }

private:
    static void Show( 
        const std::wstring& msg, 
        const std::wstring& title, 
        MsgType type, 
        HWND parent = NULL )
    {
        UINT uType = MB_ICONERROR;
        if (type == Warning)
            uType = MB_ICONWARNING;
        else if (uType == Info)
            uType = MB_ICONINFORMATION;

        MessageBoxW( parent, msg.c_str(), title.c_str(), uType );
    }
};