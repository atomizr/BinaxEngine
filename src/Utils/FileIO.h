#pragma once

#include <cstdio>
#include <string>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
   
   
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace Binax {




inline std::FILE* OpenFileUtf8(const std::string& pathUtf8)
{
#ifdef _WIN32
    if (pathUtf8.empty())
        return nullptr;

    const int size = MultiByteToWideChar(
        CP_UTF8, 0, pathUtf8.c_str(),
        static_cast<int>(pathUtf8.size()), nullptr, 0);
    if (size <= 0)
        return nullptr;

    std::wstring wpath(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, pathUtf8.c_str(),
        static_cast<int>(pathUtf8.size()), wpath.data(), size);

    return _wfopen(wpath.c_str(), L"rb");
#else
    return std::fopen(pathUtf8.c_str(), "rb");
#endif
}

} 