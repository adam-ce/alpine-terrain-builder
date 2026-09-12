#include "gdal/cli.h"
#include "log.h"
#include "tiles/cli.h"
#include <CLI/CLI.hpp>
#include <atomic>
#include <csignal>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace {
static_assert(std::atomic<int>::is_always_lock_free);
std::atomic<int> interrupted = 0;
void request_stop(int signal) { interrupted.store(signal, std::memory_order_relaxed); }
} // namespace

int main(int argc, char** argv)
{
    CLI::App app { "Import a GDAL dataset or online JPEG tiles into an immutable RF snapshot" };
    rf_builder::gdal::Options options;
    rf_builder::tiles::Options online;
    app.require_subcommand(1, 1);
    auto* gdal = app.add_subcommand("gdal", "Import a prepared GDAL dataset");
    rf_builder::gdal::cli::configure(*gdal, options);
    auto* tiles = app.add_subcommand("tiles", "Import an online JPEG tile pyramid");
    rf_builder::tiles::cli::configure(*tiles, online);
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }
    const std::string mode = options.mode == rf_builder::gdal::Mode::Colour ? "rgb" : "scalar";
    const auto output_path = tiles->parsed() ? online.output.output : options.output;
    try {
        auto log_path = output_path;
        log_path += ".log";
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), false);
        file_sink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");
        Log::get_logger()->sinks().push_back(std::move(file_sink));
        Log::get_logger()->flush_on(spdlog::level::info);
        if (tiles->parsed()) {
            LOG_INFO("RF import: provider={}, mask={}, output={}, mode=rgb, tile size={}, attribution={}; log={}",
                online.provider.string(),
                online.mask,
                output_path.string(),
                online.output.tile_side,
                online.output.attribution_index,
                log_path.string());
        } else {
            LOG_INFO("RF import: dataset={}, mask={}, output={}, mode={}, tile size={}, attribution={}; log={}",
                options.dataset,
                options.mask,
                output_path.string(),
                mode,
                options.tile_side,
                options.attribution_index,
                log_path.string());
        }
        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);
        const auto stop = [] { return interrupted.load(std::memory_order_relaxed) != 0; };
        auto result = tiles->parsed() ? rf_builder::tiles::build(online, stop) : rf_builder::gdal::build(options, stop);
        if (!result) {
            if (result.error().code() == Error::Code::Cancelled) {
                LOG_INFO("{}", result.error().to_string());
                const auto signal = interrupted.load(std::memory_order_relaxed);
                return signal ? 128 + signal : 1;
            }
            LOG_ERROR("{}", result.error().to_string());
            return 1;
        }
        LOG_INFO("Published {}: {} tiles, {}x{} pixels per tile, {} payload bytes ({} reused tiles)",
            output_path.string(),
            result->tile_count,
            result->tile_side,
            result->tile_side,
            result->tile_bytes,
            result->reused_tiles);
        return 0;
    } catch (const std::exception& error) {
        LOG_ERROR("RF import failed: {}", error.what());
        return 1;
    }
}
