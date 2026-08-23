/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
 * Copyright (C) 2025 Martin Braunsperger <e11909911@student.tuwien.ac.at>
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

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>
#include <radix/raster.h>

namespace image {

void save_image_as_png(const radix::Raster<glm::u8vec3>& image, const std::string& path);

template <typename T>
void debug_out(const radix::Raster<T>& image, const std::string& path)
{
    if (image.buffer().empty())
        throw std::invalid_argument("Can't write an empty raster to " + path);

    const auto [min, max] = std::ranges::minmax(image);
    const auto range = float(max) - float(min);
    save_image_as_png(radix::raster::transform(image,
                          [min, range](const auto value) {
                              const auto intensity = range == 0.F ? std::uint8_t(0) : std::uint8_t(255.F * (float(value) - float(min)) / range);
                              return glm::u8vec3(intensity);
                          }),
        path);
}

} // namespace image
