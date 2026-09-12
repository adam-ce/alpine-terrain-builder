#pragma once

#include "build.h"
#include "io/envelope.h"
#include "raster_store/attribution.h"
#include "planning.h"
#include <gdal_version.h>

namespace rf_builder::gdal::inputs {

inline constexpr std::string_view file_name = "inputs.tmp";

struct Record {
    std::string dataset;
    std::string mask;
    std::vector<unsigned> bands;
    Mode mode;
    std::uint32_t attribution_index;
    unsigned tile_side;
    raster_store::attribution::Entity attribution{};
    double sampling_limit = planning::sampling_limit;
    double mask_simplification_metres = 0.1;
    std::string resampling = "lanczos/base/exact/all-channels-valid";
    std::uint32_t processing_version = 1;
    std::uint32_t gdal_version = GDAL_VERSION_NUM;

    bool operator==(const Record&) const = default;
};

using Schema = io::envelope::PayloadSchema<"rf_builder.Inputs", io::envelope::Version<1, Record>>;

using run::check_link_filesystem;
using run::gdal_identifier;
using run::identifier;
Expected<void> validate_cache(const std::filesystem::path& path, const Record& record);

} // namespace rf_builder::gdal::inputs
