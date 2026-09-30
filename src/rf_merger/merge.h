#pragma once

#include "Error.h"
#include "io/compression.h"
#include "statistics.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

namespace rf_merger::merge {

struct Options {
    std::filesystem::path left;
    std::filesystem::path right;
    std::filesystem::path priorities;
    std::filesystem::path output;
    unsigned jobs = 1;
    std::optional<std::filesystem::path> cache = std::nullopt;
    io::envelope::CompressionAlgorithm compression_algorithm = io::envelope::CompressionAlgorithm::ZstdDefaultCompression;
};

struct Report {
    statistics::Totals statistics;
    bool statistics_complete = true;
    std::uint64_t restored_tiles = 0;
    unsigned tile_side = 0;
};

// Merges two published RF snapshots into a new published snapshot at
// options.output. Throws Error::Exception for invalid inputs and failures; a
// failure during production retains the incomplete .part snapshot with its
// completed tiles indexed. When stop_requested returns true, active tiles are
// finished and saved, then an Error::Exception with code Cancelled is thrown.
Report run(const Options& options, const std::function<bool()>& stop_requested = {});

} // namespace rf_merger::merge
