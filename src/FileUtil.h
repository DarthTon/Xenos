#pragma once

#include <stdint.h>
#include <string>
#include <list>
#include <vector>
#include <memory>

#define ACUT_SLASH L'\\'
#define ACUT_SLASH_STR L"\\"
#define ACUT_SLASH_UTF8 '\\'
#define ACUT_SLASH_STR_UTF8 "\\"

namespace acut
{
    // suitable only for reading small text files into a buffer
    bool read_file(const std::wstring& path, std::string& buffer);
    bool read_file(const std::wstring& path, std::vector<char>& buffer);
    bool read_file(const std::wstring& path, std::wstring& buffer);
    bool read_file(const std::wstring& path, std::vector<wchar_t>& buffer);

    std::wstring full_path(const std::wstring& filename);
    bool file_exists(const std::wstring& filename);
}


