/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include "alpine_raster.h"

#include <algorithm>
#include <execution>
#include <filesystem>
#include <stdexcept>

#include <fmt/core.h>
#include <memory>

#include "ParallelTileGenerator.h"
#include "ctb/Grid.hpp"
#include "image_writer.h"
#include <radix/raster.h>
#include <radix/height_encoding.h>

ParallelTileGenerator alpine_raster::make_generator(const std::string& input_data_path, const std::string& output_data_path, ctb::Grid::Srs srs, radix::tile::Border border, unsigned grid_resolution)
{
    return ParallelTileGenerator::make(input_data_path, srs, std::make_unique<alpine_raster::TileWriter>(border), output_data_path, grid_resolution);
}

void alpine_raster::TileWriter::write(const std::string& file_path, const radix::tile::Descriptor&, const radix::Raster<float>& heights) const
{
    auto result = image::save_image_as_png(radix::raster::transform(heights, radix::height_encoding::to_rgb), file_path);
    if (!result) {
        throw std::runtime_error(result.error().to_string());
    }
}
