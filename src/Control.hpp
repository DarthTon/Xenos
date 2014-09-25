#pragma once

#include "Window.hpp"

namespace ctrl
{

class Control : public Window
{
public:
    Control( HWND hwnd = NULL )
        : Window( hwnd )  {  }
};

}