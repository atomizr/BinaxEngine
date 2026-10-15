
#include "Log.h"

namespace Binax {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

void Log::Init()
{
    
    
    
    s_CoreLogger = spdlog::stdout_color_mt("BINAX");
    s_CoreLogger->set_level(spdlog::level::trace);
    s_CoreLogger->set_pattern("%^[%T] %n: %v%$");
}

} 