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
    io::envelope::CompressionAlgorithm compression_algorithm = io::envelope::CompressionAlgorithm::ZstdDefaultCompressionWithChecksum;
    io::envelope::ChecksumAlgorithm checksum_algorithm = io::envelope::ChecksumAlgorithm::HandledByCompressionLib;
};

struct Report {
    statistics::Totals statistics;
    bool statistics_complete = true;
    std::uint64_t restored_tiles = 0;
    unsigned tile_side = 0;
};

// Merges two published RF snapshots into a new published snapshot at
// options.output. Cancellation retains the incomplete .part snapshot.
Expected<Report> run(const Options& options, const std::function<bool()>& stop_requested = {});

} // namespace rf_merger::merge
