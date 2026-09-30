/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#ifndef SPDLOG_FMT_EXTERNAL
#define SPDLOG_FMT_EXTERNAL
#define FMT_HEADER_ONLY
#endif
#include "fmt_impls.h"
#include "numeric/int_math.h"
#include <fmt/core.h>
#include <spdlog/spdlog.h>

class Log {
private:
	static std::shared_ptr<spdlog::logger> logger;
public:
	static void init(spdlog::level::level_enum level);
	inline static std::shared_ptr<spdlog::logger>& get_logger() {
        if (Log::logger == nullptr) {
            Log::init(spdlog::level::level_enum::info);
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

// Logs at most once per call site for the lifetime of the process.
#define LOG_WARN_ONCE(...)                  \
    do {                                     \
        static bool _log_warn_once_fired = false; \
        if (!_log_warn_once_fired) {         \
            LOG_WARN(__VA_ARGS__);           \
            _log_warn_once_fired = true;     \
        }                                    \
    } while (false)

// Tracks call count per call site and reports whether the current call falls
// on a power-of-two occurrence (1, 2, 4, 8, 16, ...).
class LogBackoff {
public:
    bool should_log() {
        this->_count++;
        return is_power_of_two(this->_count);
    }

    uint64_t count() const {
        return this->_count;
    }

private:
    uint64_t _count = 0;
};

// Logs on occurrence 1, 2, 4, 8, 16, ... per call site, with the occurrence count appended.
#define LOG_WARN_BACKOFF(fmt_str, ...)                                  \
    do {                                                                \
        static LogBackoff _log_warn_backoff;                            \
        if (_log_warn_backoff.should_log()) {                           \
            LOG_WARN(fmt_str " (occurrence {})", ##__VA_ARGS__, _log_warn_backoff.count()); \
        }                                                                \
    } while (false)
