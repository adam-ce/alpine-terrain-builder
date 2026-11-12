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

#include <utility>

#include <radix/geometry.h>

#include "Error.h"
#include "raster_store/AttributedRaster.h"

namespace raster_store {

inline constexpr unsigned default_tile_side = 4096;

inline Expected<void> validate_dimensions(const glm::uvec2 dimensions)
{
    if (dimensions.x == 0 || dimensions.x != dimensions.y) {
        return Error::fail(Error::Code::InvalidInput, "raster tile dimensions must be positive and square");
    }
    return {};
}

// A whole stored tile including its halo, the store's NodeData.
template <typename PixelType>
struct Tile : AttributedRaster<PixelType> {
    // A half-open [min, max) region in stored pixels, including the halo.
    using Part = radix::geometry::Aabb2ui;

    using AttributedRaster<PixelType>::AttributedRaster;

    explicit Tile(const unsigned side = default_tile_side)
        : AttributedRaster<PixelType>(glm::uvec2(side))
    {
    }

    explicit Tile(AttributedRaster<PixelType> raster)
        : AttributedRaster<PixelType>(std::move(raster))
    {
    }
};

} // namespace raster_store
