
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace Binax {

class Log
{
public:
    static void Init();
    static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
};

} 


#define BINAX_LOG_TRACE(...)    ::Binax::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define BINAX_LOG_INFO(...)     ::Binax::Log::GetCoreLogger()->info(__VA_ARGS__)
#define BINAX_LOG_WARN(...)     ::Binax::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define BINAX_LOG_ERROR(...)    ::Binax::Log::GetCoreLogger()->error(__VA_ARGS__)
#define BINAX_LOG_CRITICAL(...) ::Binax::Log::GetCoreLogger()->critical(__VA_ARGS__)