#pragma once

#include "Error.h"
#include "raster_store/pixel.h"
#include "run.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rf_builder::gdal {

enum class Mode : std::uint8_t { Scalar, Colour };

struct Options {
    std::string dataset;
    std::string mask;
    std::filesystem::path output;
    std::uint32_t attribution_index = 0;
    Mode mode = Mode::Scalar;
    std::vector<unsigned> bands;
    unsigned tile_side = 4096;
    unsigned jobs = 1;
    std::optional<std::filesystem::path> cache = std::nullopt;
    std::optional<raster_store::pixel::Mapping> value_mapping = std::nullopt;
    unsigned nodata_search_radius = 5;
    unsigned nodata_smoothing_kernel_size = 5;
    std::vector<double> nodata_default_value { 0 };
};

using Report = run::Report;

// stop_requested is polled only by the calling/coordinator thread. Active tiles
// finish on cancellation; their results are saved and checkpointed, not published.
Expected<Report> build(const Options& options, const std::function<bool()>& stop_requested = {});

} // namespace rf_builder::gdal
