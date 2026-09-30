/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include "build.h"
#include "io/envelope.h"
#include "planning.h"
#include "raster_store/attribution.h"
#include "raster_store/pixel.h"
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
    std::uint32_t processing_version = 2;
    std::uint32_t gdal_version = GDAL_VERSION_NUM;

    raster_store::pixel::Mapping value_mapping = raster_store::pixel::Mapping::Linear;
    unsigned nodata_search_radius = 5;
    unsigned nodata_smoothing_kernel_size = 5;
    std::array<float, 3> nodata_default_value {};

    bool operator==(const Record&) const = default;
};

using Schema = io::envelope::PayloadSchema<"rf_builder.Inputs", io::envelope::Version<2, Record>>;

using run::check_link_filesystem;
using run::gdal_identifier;
using run::identifier;
Expected<void> validate_cache(const std::filesystem::path& path, const Record& record);

} // namespace rf_builder::gdal::inputs
