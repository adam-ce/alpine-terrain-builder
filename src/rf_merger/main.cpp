#include "cli.h"
#include "log.h"
#include <CLI/CLI.hpp>
#include <atomic>
#include <csignal>
#include <spdlog/sinks/basic_file_sink.h>

namespace {
static_assert(std::atomic<int>::is_always_lock_free);
std::atomic<int> interrupted = 0;
void request_stop(int signal) { interrupted.store(signal, std::memory_order_relaxed); }
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
        const auto stop = [] { return interrupted.load(std::memory_order_relaxed) != 0; };
        auto result = rf_merger::merge::run(options, stop);
        if (!result) {
            if (result.error().code() == Error::Code::Cancelled) {
                LOG_INFO("{}", result.error().to_string());
                const auto signal = interrupted.load(std::memory_order_relaxed);
                return signal ? 128 + signal : 1;
            }
            LOG_ERROR("{}", result.error().to_string());
            return 1;
        }
        LOG_INFO("Published {}: {} tiles, {}x{} pixels per tile ({} restored from cache)",
            options.output.string(),
            result->statistics.total().tiles,
            result->tile_side,
            result->tile_side,
            result->restored_tiles);
        for (const auto& line : rf_merger::statistics::format(result->statistics, result->statistics_complete)) {
            LOG_INFO("{}", line);
        }
        return 0;
    } catch (const std::exception& error) {
        LOG_ERROR("RF merge failed: {}", error.what());
        return 1;
    }
}
