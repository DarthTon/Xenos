#pragma once

#include "Dialog.hpp"
#include "resource.h"
#include "InjectionCore.h"

#include <thread>
#include <future>

class DlgWait : public Dialog
{
public:
    DlgWait( InjectionCore& core, InjectContext& context );
    ~DlgWait();

    inline NTSTATUS status() const { return _status; }

private:
    /// <summary>
    /// Wait for injection
    /// </summary>
    /// <returns>Error code</returns>
    NTSTATUS WaitForInjection();

    MSG_HANDLER( OnInit );
    MSG_HANDLER( OnCloseBtn );

private:
    InjectionCore& _core;
    InjectContext& _context;
    std::thread _waitThread;
    NTSTATUS _status = STATUS_SUCCESS;
};

