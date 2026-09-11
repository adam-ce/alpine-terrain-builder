#include "build.h"
#include "log.h"
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
    CLI::App app { "Import one prepared GDAL dataset into an immutable RF snapshot" };
    rf_builder::Options options;
    std::string mode = "scalar";
    std::string cache;
    app.add_option("--dataset", options.dataset, "Prepared raster or VRT path/URL")->required();
    app.add_option("--mask", options.mask, "Vector validity mask (split polygons at the antimeridian)")->required();
    app.add_option("--output", options.output, "New final snapshot path; runtime log appends to <output>.log")->required();
    app.add_option("--attribution-index", options.attribution_index, "Existing attribution entry (1..65534)")->required();
    app.add_option("--mode", mode, "Output representation")->check(CLI::IsMember({ "scalar", "rgb" }))->default_val("scalar");
    app.add_option("--bands", options.bands, "One scalar band or three bands in RGB order")->expected(1, 3);
    app.add_option("--tile-size", options.tile_side, "Pixels per side")->check(CLI::PositiveNumber)->default_val(4096);
    app.add_option("--jobs", options.jobs, "Concurrent tile workers")->check(CLI::PositiveNumber)->default_val(1);
    app.add_option("--cache", cache, "Compatible incomplete .part snapshot to reuse");
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }
    options.mode = mode == "rgb" ? rf_builder::Mode::Colour : rf_builder::Mode::Scalar;
    if (!cache.empty()) { options.cache = cache; }
    try {
        auto log_path = options.output;
        log_path += ".log";
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), false);
        file_sink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");
        Log::get_logger()->sinks().push_back(std::move(file_sink));
        Log::get_logger()->flush_on(spdlog::level::info);
        LOG_INFO("RF import: dataset={}, mask={}, output={}, mode={}, tile size={}, attribution={}; log={}",
            options.dataset, options.mask, options.output.string(), mode, options.tile_side, options.attribution_index, log_path.string());
        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);
        auto result = rf_builder::build(options, [] { return interrupted.load(std::memory_order_relaxed) != 0; });
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
            options.output.string(), result->tile_count, result->tile_side, result->tile_side, result->tile_bytes, result->reused_tiles);
        return 0;
    } catch (const std::exception& error) {
        LOG_ERROR("RF import failed: {}", error.what());
        return 1;
    }
}
