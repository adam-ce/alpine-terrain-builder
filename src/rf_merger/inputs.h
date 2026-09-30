#pragma once

#include "Error.h"
#include "io/envelope.h"
#include "raster_store/pixel.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rf_merger::inputs {

inline constexpr std::string_view file_name = "inputs.tmp";

// Identifies an input dataset by the recorded payload hashes of its metadata and index.
struct Fingerprint {
    std::string path;
    std::vector<std::byte> metadata_hash;
    std::vector<std::byte> index_hash;

    bool operator==(const Fingerprint&) const = default;
};

struct Record {
    Fingerprint left;
    Fingerprint right;
    std::vector<std::uint16_t> priorities;
    std::string payload_type;
    unsigned nominal_tile_size = 0;
    unsigned stored_tile_size = 0;
    unsigned halo_width = 0;
    raster_store::pixel::Mapping value_mapping = raster_store::pixel::Mapping::Linear;
    std::string resampling = "lanczos3";
    std::string selection = "whole-tile-attribution/pixel-priority/zoom/right";
    std::string partitioning = "input-topology";
    // Covers the shared scaling and halo contracts; bump on semantic changes.
    std::uint32_t processing_version = 1;

    bool operator==(const Record&) const = default;
};

using Schema = io::envelope::PayloadSchema<"rf_merger.Inputs", io::envelope::Version<1, Record>>;

Expected<Fingerprint> fingerprint(const std::filesystem::path& snapshot);
Expected<void> validate_cache(const std::filesystem::path& path, const Record& record);

} // namespace rf_merger::inputs
