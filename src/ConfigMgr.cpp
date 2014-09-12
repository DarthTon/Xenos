#include "ConfigMgr.h"
#include "../../BlackBone/src/BlackBone/Misc/Utils.h"

#define CONFIG_FILE L"\\XenosConfig.xml"

bool ConfigMgr::Save( const ConfigData& data )
{
    try
    {
        acut::XmlDoc<wchar_t> xml;
        xml.create_document();

        xml.set( L"XenosConfig.imagePath",      data.imagePath.c_str() );
        xml.set( L"XenosConfig.manualMapFlags", data.manualMapFlags );
        xml.set( L"XenosConfig.newProcess",     data.newProcess );
        xml.set( L"XenosConfig.procName",       data.procName.c_str() );
        xml.set( L"XenosConfig.threadHijack",   data.threadHijack );
        xml.set( L"XenosConfig.unlink",         data.unlink );
        xml.set( L"XenosConfig.close",          data.close );
        xml.set( L"XenosConfig.injectMode",     data.injectMode );
        xml.set( L"XenosConfig.procCmdLine",    data.procCmdLine.c_str() );
        xml.set( L"XenosConfig.initRoutine",    data.initRoutine.c_str() );
        xml.set( L"XenosConfig.initArgs",       data.initArgs.c_str() );

        xml.write_document( blackbone::Utils::GetExeDirectory() + CONFIG_FILE );
        
        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

bool ConfigMgr::Load( ConfigData& data )
{
    try
    {
        auto path = blackbone::Utils::GetExeDirectory() + CONFIG_FILE;
        if (!acut::file_exists( path ))
            return false;

        acut::XmlDoc<wchar_t> xml;
        xml.read_from_file( path );

        xml.get_if_present( L"XenosConfig.imagePath",       data.imagePath );
        xml.get_if_present( L"XenosConfig.manualMapFlags",  data.manualMapFlags );
        xml.get_if_present( L"XenosConfig.newProcess",      data.newProcess );
        xml.get_if_present( L"XenosConfig.procName",        data.procName );
        xml.get_if_present( L"XenosConfig.threadHijack",    data.threadHijack );
        xml.get_if_present( L"XenosConfig.unlink",          data.unlink );
        xml.get_if_present( L"XenosConfig.close",           data.close );
        xml.get_if_present( L"XenosConfig.injectMode",      data.injectMode );
        xml.get_if_present( L"XenosConfig.procCmdLine",     data.procCmdLine );
        xml.get_if_present( L"XenosConfig.initRoutine",     data.initRoutine );
        xml.get_if_present( L"XenosConfig.initArgs",        data.initArgs );

        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}
