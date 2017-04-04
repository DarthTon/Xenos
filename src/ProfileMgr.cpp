#include "ProfileMgr.h"
#include <BlackBone/src/BlackBone/Misc/Utils.h>

#define CURRENT_PROFILE L"\\XenosCurrentProfile.xpr"

bool ProfileMgr::Save( const std::wstring& path /*= L""*/)
{
    try
    {
        auto filepath = path.empty() ? (blackbone::Utils::GetExeDirectory() + CURRENT_PROFILE) : path;

        acut::XmlDoc<wchar_t> xml;
        xml.create_document();

        for (auto& imgpath : _config.images)
            xml.append( L"XenosConfig.imagePath" ).value( imgpath );

        xml.set( L"XenosConfig.manualMapFlags", _config.mmapFlags );
        xml.set( L"XenosConfig.procName",       _config.procName.c_str() );
        xml.set( L"XenosConfig.hijack",         _config.hijack );
        xml.set( L"XenosConfig.unlink",         _config.unlink );
        xml.set( L"XenosConfig.erasePE",        _config.erasePE );
        xml.set( L"XenosConfig.close",          _config.close );
        xml.set( L"XenosConfig.krnHandle",      _config.krnHandle );
        xml.set( L"XenosConfig.injIndef",       _config.injIndef );
        xml.set( L"XenosConfig.processMode",    _config.processMode );
        xml.set( L"XenosConfig.injectMode",     _config.injectMode );
        xml.set( L"XenosConfig.delay",          _config.delay );
        xml.set( L"XenosConfig.period",         _config.period );
        xml.set( L"XenosConfig.skip",           _config.skipProc );
        xml.set( L"XenosConfig.procCmdLine",    _config.procCmdLine.c_str() );
        xml.set( L"XenosConfig.initRoutine",    _config.initRoutine.c_str() );
        xml.set( L"XenosConfig.initArgs",       _config.initArgs.c_str() );

        xml.write_document( filepath );
        
        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

bool ProfileMgr::Load( const std::wstring& path /*= L""*/ )
{
    try
    {
        auto filepath = path.empty() ? (blackbone::Utils::GetExeDirectory() + CURRENT_PROFILE) : path;
        if (!acut::file_exists( filepath ))
            return false;

        acut::XmlDoc<wchar_t> xml;
        xml.read_from_file( filepath );

        // Load images in a safe way
        if(xml.has( L"XenosConfig.imagePath" ))
        {
            auto nodes = xml.all_nodes_named( L"XenosConfig.imagePath" );
            for (auto& node : nodes)
                _config.images.emplace_back( node.value() );
        }

        xml.get_if_present( L"XenosConfig.manualMapFlags",  _config.mmapFlags );
        xml.get_if_present( L"XenosConfig.procName",        _config.procName );
        xml.get_if_present( L"XenosConfig.hijack",          _config.hijack );
        xml.get_if_present( L"XenosConfig.unlink",          _config.unlink );
        xml.get_if_present( L"XenosConfig.erasePE",         _config.erasePE );
        xml.get_if_present( L"XenosConfig.close",           _config.close );
        xml.get_if_present( L"XenosConfig.krnHandle",       _config.krnHandle );
        xml.get_if_present( L"XenosConfig.injIndef",        _config.injIndef );
        xml.get_if_present( L"XenosConfig.processMode",     _config.processMode );
        xml.get_if_present( L"XenosConfig.injectMode",      _config.injectMode );
        xml.get_if_present( L"XenosConfig.delay",           _config.delay );
        xml.get_if_present( L"XenosConfig.period",          _config.period );
        xml.get_if_present( L"XenosConfig.skip",            _config.skipProc );
        xml.get_if_present( L"XenosConfig.procCmdLine",     _config.procCmdLine );
        xml.get_if_present( L"XenosConfig.initRoutine",     _config.initRoutine );
        xml.get_if_present( L"XenosConfig.initArgs",        _config.initArgs );

        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}
