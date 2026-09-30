/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include "cli.h"
#include "log.h"
#include <CLI/CLI.hpp>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <libassert/assert.hpp>
#include <spdlog/sinks/basic_file_sink.h>

namespace {
static_assert(std::atomic<int>::is_always_lock_free);
std::atomic<int> interrupted = 0;
void request_stop(int signal) { interrupted.store(signal, std::memory_order_relaxed); }

// Assertion failures are merger bugs; record them with their stack trace in
// the log file as well as on stderr.
void log_assertion_failure(const libassert::assertion_info& info)
{
    LOG_ERROR("{}", info.to_string(0, libassert::color_scheme::blank));
    Log::get_logger()->flush();
    std::abort();
}
} // namespace

int main(int argc, char** argv)
{
    CLI::App app { "Merge two RF snapshots into a new immutable RF snapshot by attribution priority" };
    rf_merger::merge::Options options;
    rf_merger::cli::configure(app, options);
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }
    try {
        auto log_path = options.output;
        log_path += ".log";
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), false);
        file_sink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");
        Log::get_logger()->sinks().push_back(std::move(file_sink));
        Log::get_logger()->flush_on(spdlog::level::info);
        libassert::set_failure_handler(log_assertion_failure);
        LOG_INFO("RF merge: left={}, right={}, priorities={}, output={}, jobs={}, cache={}; log={}",
            options.left.string(),
            options.right.string(),
            options.priorities.string(),
            options.output.string(),
            options.jobs,
            options.cache ? options.cache->string() : "none",
            log_path.string());
        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);
        const auto report = rf_merger::merge::run(options, [] { return interrupted.load(std::memory_order_relaxed) != 0; });
        LOG_INFO("Published {}: {} tiles, {}x{} pixels per tile ({} restored from cache)",
            options.output.string(),
            report.statistics.total().tiles,
            report.tile_side,
            report.tile_side,
            report.restored_tiles);
        for (const auto& line : rf_merger::statistics::format(report.statistics, report.statistics_complete)) {
            LOG_INFO("{}", line);
        }
        return 0;
    } catch (const Error::Exception& exception) {
        if (exception.error().code() == Error::Code::Cancelled) {
            LOG_INFO("{}", exception.error().to_string());
            const auto signal = interrupted.load(std::memory_order_relaxed);
            return signal ? 128 + signal : 1;
        }
        LOG_ERROR("{}\nStack trace where the error was made:\n{}", exception.error().to_string(), exception.error().stacktrace());
        return 1;
    } catch (const std::exception& error) {
        LOG_ERROR("RF merge failed: {}", error.what());
        return 1;
    }
}
