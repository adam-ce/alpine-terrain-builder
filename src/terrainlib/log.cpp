#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "log.h"

std::shared_ptr<spdlog::logger> Log::logger;

void Log::init(spdlog::level::level_enum log_level) {
	spdlog::set_pattern("[%Y-%m-%d %T.%e] [%=3n] [%^%l%$] %v");
	Log::logger = spdlog::stderr_color_mt("LOG");
	Log::logger->set_level(log_level);
}
