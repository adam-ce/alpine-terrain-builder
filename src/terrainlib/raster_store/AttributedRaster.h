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

#include <cstdint>

#include <glm/glm.hpp>
#include <radix/raster.h>

namespace raster_store {

// A data raster and the attribution of each of its pixels.
template <typename PixelType>
struct AttributedRaster {
    explicit AttributedRaster(const unsigned side)
        : AttributedRaster(glm::uvec2(side))
    {
    }

    explicit AttributedRaster(const glm::uvec2 size)
        : data(size)
        , attribution(size, std::uint16_t { 0 })
    {
    }

    radix::Raster<PixelType> data;
    radix::Raster<std::uint16_t> attribution;
};

} // namespace raster_store
