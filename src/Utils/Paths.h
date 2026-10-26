#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <limits.h>
#  include <unistd.h>
#endif

namespace Binax::Paths {




inline std::string ToUtf8(const std::filesystem::path& p)
{
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

inline std::filesystem::path FromUtf8(const std::string& s)
{
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}


inline std::filesystem::path GetExecutableDir()
{
#ifdef _WIN32
    
    wchar_t buf[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buf,
                                       static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0]))
        return {};
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
#else
    char buf[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    return std::filesystem::path(std::string(buf)).parent_path();
#endif
}









inline std::string Resolve(const std::string& relativeOrAbsolute)
{
    if (relativeOrAbsolute.empty())
        return relativeOrAbsolute;

    std::error_code ec;
    const auto asGiven = FromUtf8(relativeOrAbsolute);
    if (std::filesystem::exists(asGiven, ec) && !ec)
        return relativeOrAbsolute;

    const auto exeDir = GetExecutableDir();
    if (!exeDir.empty())
    {
        const auto candidate = exeDir / asGiven;
        if (std::filesystem::exists(candidate, ec) && !ec)
            return ToUtf8(candidate);
    }

    return relativeOrAbsolute;
}

} 
