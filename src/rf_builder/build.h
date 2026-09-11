#pragma once

#include "Error.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rf_builder {

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
};

struct Report {
    std::uint64_t tile_count = 0;
    std::uint64_t tile_bytes = 0;
    std::uint64_t reused_tiles = 0;
    unsigned tile_side = 0;
};

// stop_requested is polled only by the calling/coordinator thread. Active tiles
// finish on cancellation; their results are saved and checkpointed, not published.
Expected<Report> build(const Options& options, const std::function<bool()>& stop_requested = {});

} // namespace rf_builder
