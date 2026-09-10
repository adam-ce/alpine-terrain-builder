#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "Error.h"

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
    std::optional<std::filesystem::path> cache = std::nullopt;
};

struct Report {
    std::uint64_t tile_count = 0;
    std::uint64_t tile_bytes = 0;
    std::uint64_t reused_tiles = 0;
    unsigned tile_side = 0;
};

Expected<Report> build(const Options& options);

} // namespace rf_builder
