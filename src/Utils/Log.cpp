#include "Log.h"
#include "Paths.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdio>
#include <string>
#include <vector>

namespace Binax {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

void Log::Init()
{
    
    
    
    if (s_CoreLogger)
        return;

    std::vector<spdlog::sink_ptr> sinks;

    
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] %n: %v%$");
    sinks.push_back(consoleSink);

    
    try
    {
        const std::filesystem::path logPath = Paths::GetExecutableDir() / "debug.log";
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            Paths::ToUtf8(logPath), true);
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

} 
