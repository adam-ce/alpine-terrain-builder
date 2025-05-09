#ifndef LOG_H
#define LOG_H

#ifndef SPDLOG_FMT_EXTERNAL
#define SPDLOG_FMT_EXTERNAL
#define FMT_HEADER_ONLY
#endif
#include <fmt/core.h>
#include <spdlog/spdlog.h>

class Log {
private:
	static std::shared_ptr<spdlog::logger> logger;
public:
	static void init(spdlog::level::level_enum logLevel);

	inline static std::shared_ptr<spdlog::logger>& get_logger() {
        if (Log::logger == nullptr) {
            Log::init(spdlog::level::level_enum::trace);
        }
		return Log::logger;
	}
};

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(::Log::get_logger(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(::Log::get_logger(), __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_INFO (::Log::get_logger(), __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_WARN (::Log::get_logger(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(::Log::get_logger(), __VA_ARGS__)
#define LOG_ERROR_AND_EXIT(...) \
    do {                        \
        LOG_ERROR(__VA_ARGS__); \
        exit(1);                \
    } while (false)
#if defined(__GNUC__) || defined(__clang__)
#define UNREACHABLE()                                                       \
    do {                                                                    \
        LOG_ERROR("Reached unreachable code at %s:%d", __FILE__, __LINE__); \
        __builtin_unreachable();                                            \
    } while (0)
#elif defined(_MSC_VER)
#define UNREACHABLE()                                                       \
    do {                                                                    \
        LOG_ERROR("Reached unreachable code at %s:%d", __FILE__, __LINE__); \
        __assume(false);                                                    \
    } while (0)
#else
#define UNREACHABLE()                                                       \
    do {                                                                    \
        LOG_ERROR("Reached unreachable code at %s:%d", __FILE__, __LINE__); \
        std::abort();                                                       \
    } while (0)
#endif

#endif
