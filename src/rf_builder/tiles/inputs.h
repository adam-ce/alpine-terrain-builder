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
#include "io/envelope.h"
#include "provider.h"
#include "raster_store/attribution.h"
#include "raster_store/pixel.h"
#include <gdal_version.h>
namespace rf_builder::tiles::inputs {
struct Record {
    provider::Settings provider;
    unsigned tile_side = 4096;
    std::string mask;
    unsigned attribution_index = 0;
    raster_store::attribution::Entity attribution;
    double mask_simplification_metres = 0.1;
    std::string decoding = "opencv/jpeg8/ignore-orientation/rgb/no-icc";
    std::string fallback = "lanczos3/srgb-linear-light/ancestor-neighbours/wrap-x/extend-edges";
    std::uint32_t processing_version = 1;
    std::uint32_t gdal_version = GDAL_VERSION_NUM;
    raster_store::pixel::Mapping value_mapping = raster_store::pixel::Mapping::SRGBA;

    bool operator==(const Record&) const = default;
};
using Schema = io::envelope::PayloadSchema<"rf_builder.tiles.Inputs", io::envelope::Version<2, Record>>;
Expected<void> validate_cache(const std::filesystem::path& path, const Record& record);
} // namespace rf_builder::tiles::inputs
