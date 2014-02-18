#include "FileUtil.h"
#include "StringUtil.h"

#include <fstream>

#include <ShlObj.h>

template <typename Container>
bool read_helper(const std::wstring& path, Container& container)
{
    std::basic_ifstream<typename Container::value_type> f(path, std::ios_base::binary);

    if (!f.good())
        return false;

    container.assign((std::istreambuf_iterator<typename Container::value_type>(f)),
                      std::istreambuf_iterator<typename Container::value_type>());
    container.push_back(acut::ensure_tchar<typename Container::value_type>('\0'));

    return true;
}


bool acut::read_file(const std::wstring& path, std::string& buffer)
{
    return read_helper(path, buffer);
}

bool acut::read_file(const std::wstring& path, std::vector<char>& buffer)
{
    return read_helper(path, buffer);
}

bool acut::read_file(const std::wstring& path, std::wstring& buffer)
{
    return read_helper(path, buffer);
}

bool acut::read_file(const std::wstring& path, std::vector<wchar_t>& buffer)
{
    return read_helper(path, buffer);
}

bool acut::file_exists( const std::wstring& filename )
{
    return (GetFileAttributesW( filename.c_str() ) != INVALID_FILE_ATTRIBUTES);
}

