#pragma once

#include <fstream>
#include <cstdarg>
#include <ctime>
#include <cstdio>

namespace xlog
{

namespace LogLevel
{
    enum e
    {
        fatal = 0,
        error,
        critical,
        warning,
        normal,
        verbose
    };
}

static const char* sLogLevel[] = { "*FATAL*", "*ERROR*", "*CRITICAL*", "*WARNING*", "*NORMAL*", "*VERBOSE*" };

/// <summary>
/// Primitive log subsystem
/// </summary>
class Logger
{
public:
    ~Logger()
    {
        _output << std::endl;
    }

    static Logger& Instance()
    {
        static Logger log;
        return log;
    }

    bool DoLog( LogLevel::e level, const char* fmt, ... )
    {
        va_list alist;
        bool result = false;

        va_start( alist, fmt );
        result = DoLogV( level, fmt, alist );
        va_end( alist );

        return result;
    }

    bool DoLogV( LogLevel::e level, const char* fmt, va_list vargs )
    {
        char varbuf[2048] = { 0 };
        char message[4096] = { 0 };
        char timebuf[256] = { 0 };

        // Format message time
        auto t = std::time( nullptr );
        tm stm;
        localtime_s( &stm, &t );
        std::strftime( timebuf, _countof( timebuf ), "%Y-%m-%d %H:%M:%S", &stm );

        // Format messages
        vsprintf_s( varbuf, _countof( varbuf ), fmt, vargs );
        sprintf_s( message, _countof( message ), "%s %-12s %s", timebuf, sLogLevel[level], varbuf );

        _output << message << std::endl;

        return true;
    }

private:
    Logger()
    {
        _output.open( "Xenos.log", std::ios::out | std::ios::app );
    }

    Logger( const Logger& ) = delete;
    Logger& operator = (const Logger&) = delete;

private:
    std::ofstream _output;
};


inline bool Fatal( const char* fmt, ... )
{
    va_list alist;
    bool result = false;

    va_start( alist, fmt );
    result = Logger::Instance().DoLogV( LogLevel::fatal, fmt, alist );
    va_end( alist );

    return result;
}

inline bool Error( const char* fmt, ... )
{
    va_list alist;
    bool result = false;

    va_start( alist, fmt );
    result = Logger::Instance().DoLogV( LogLevel::error, fmt, alist );
    va_end( alist );

    return result;
}

inline bool Critical( const char* fmt, ... )
{
    va_list alist;
    bool result = false;

    va_start( alist, fmt );
    result = Logger::Instance().DoLogV( LogLevel::critical, fmt, alist );
    va_end( alist );

    return result;
}


inline bool Warning( const char* fmt, ... )
{
    va_list alist;
    bool result = false;

    va_start( alist, fmt );
    result = Logger::Instance().DoLogV( LogLevel::warning, fmt, alist );
    va_end( alist );

    return result;
}

inline bool Normal( const char* fmt, ... )
{
    va_list alist;
    bool result = false;

    va_start( alist, fmt );
    result = Logger::Instance().DoLogV( LogLevel::normal, fmt, alist );
    va_end( alist );

    return result;
}

inline bool Verbose( const char* fmt, ... )
{
    va_list alist;
    bool result = false;

    va_start( alist, fmt );
    result = Logger::Instance().DoLogV( LogLevel::verbose, fmt, alist );
    va_end( alist );

    return result;
}

}
