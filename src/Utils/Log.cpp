#include "Log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <limits.h>
#  include <unistd.h>
#endif

namespace Binax {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

namespace {

#ifdef _WIN32

// Возвращает wide-путь к каталогу exe.
std::wstring GetExecutableDirW()
{
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH)
        return L".";

    wchar_t* lastSlash = wcsrchr(buf, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(buf, L'/');
    if (!lastSlash) return L".";
    *lastSlash = L'\0';
    return std::wstring(buf);
}

// [FIX] Wide → UTF-8. spdlog с filename_t=std::string ожидает именно
//       узкую строку; UTF-8 — лучший компромисс без потери информации.
//       WideCharToMultiByte вызываем с CP_UTF8 и без MB_ERR_INVALID_CHARS,
//       поэтому функция НИКОГДА не бросает std::system_error 1113.
std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                   nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        out.data(), size, nullptr, nullptr);
    return out;
}

#else

std::string GetExecutableDir()
{
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = '\0';
    std::string path(buf);
    auto pos = path.find_last_of('/');
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

#endif

} // namespace

void Log::Init()
{
    std::vector<spdlog::sink_ptr> sinks;

    // ---- Sink 1: цветная консоль ----
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] %n: %v%$");
    sinks.push_back(consoleSink);

    // ---- Sink 2: <exe_dir>/debug.log ----
    try
    {
#ifdef _WIN32
        const std::string logPath = WideToUtf8(GetExecutableDirW()) + "/debug.log";
#else
        const std::string logPath = GetExecutableDir() + "/debug.log";
#endif
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            logPath, /*truncate=*/true);
        fileSink->set_pattern("[%Y-%m-%d %T] [%l] %n: %v");
        sinks.push_back(fileSink);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr,
                     "BINAX log: file sink init failed: %s\n", ex.what());
    }

    s_CoreLogger = std::make_shared<spdlog::logger>(
        "BINAX", sinks.begin(), sinks.end());
    s_CoreLogger->set_level(spdlog::level::trace);
    s_CoreLogger->flush_on(spdlog::level::trace);
    spdlog::register_logger(s_CoreLogger);
}

} // namespace Binax
