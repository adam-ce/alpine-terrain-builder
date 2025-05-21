#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

std::shared_ptr<spdlog::logger> Log::Logger;
std::shared_ptr<spdlog::logger> Log::GLLogger;
std::shared_ptr<spdlog::logger> Log::GLFWLogger;
std::shared_ptr<spdlog::logger> Log::PXLogger;
std::shared_ptr<spdlog::logger> Log::FTLogger;

void Log::Init(spdlog::level::level_enum logLevel)
{
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%=3n] [%^%l%$] %v");
    Logger = spdlog::stderr_color_mt("LOG");
    GLLogger = spdlog::stderr_color_mt("GL");
    GLFWLogger = spdlog::stderr_color_mt("GLFW");

    Logger->set_level(logLevel);
    GLLogger->set_level(logLevel);
    GLFWLogger->set_level(logLevel);
}
