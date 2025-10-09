/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#ifndef ALGORITHMS_RASTER_TRIANGLE_SCANLINE_H
#define ALGORITHMS_RASTER_TRIANGLE_SCANLINE_H

#include "primitives.h"
#include "tntn/Raster.h"
#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

namespace raster {

template <typename T, typename Lambda>
void triangle_scanline(const tntn::Raster<T>& raster, const glm::uvec2& a, const glm::uvec2& b, const glm::uvec2& c, const Lambda& fun)
{
    assert(primitives::winding(a, b, c) == primitives::Winding::CCW);

    const auto min = glm::min(glm::min(a, b), c);
    const auto max = glm::max(glm::max(a, b), c);
    for (auto y = min.y; y <= max.y; ++y) {
        for (auto x = min.x; x <= max.x; ++x) {
            const auto coord = glm::uvec2(x, y);
            if (primitives::inside(coord, a, b, c))
                fun(coord, raster.value(coord.y, coord.x)); // raster is row / column
        }
    }
    if (min.y == 0) {
        // bottom row of raster is not included in primitives::inside, so check for it and make an extrawurscht.
        const auto walk_bottom = [&](const glm::uvec2& a, const glm::uvec2& b) {
            if ((b - a).y == 0 && a.y == 0) {
                const auto end_x = std::max(a.x, b.x);
                for (auto x = std::min(a.x, b.x); x < end_x; ++x) {
                    const auto coord = glm::uvec2(x, 0);
                    fun(coord, raster.value(coord.y, coord.x)); // raster is row / column
                }
            }
        };
        walk_bottom(a, b);
        walk_bottom(b, c);
        walk_bottom(c, a);
    }
    const auto last_x = raster.get_width() - 1;
    if (max.x == last_x) {
        // similar with the rightmost column
        const auto walk_right = [&](const glm::uvec2& a, const glm::uvec2& b) {
            if ((b - a).x == 0 && a.x == last_x) {
                const auto end_y = std::max(a.y, b.y);
                for (auto y = std::min(a.y, b.y); y < end_y; ++y) {
                    const auto coord = glm::uvec2(last_x, y);
                    fun(coord, raster.value(coord.y, coord.x)); // raster is row / column
                }
            }
        };
        walk_right(a, b);
        walk_right(b, c);
        walk_right(c, a);
    }
    //  if (max.x == last_x && min.y == 0) {
    //    const auto check_br = [&](const glm::uvec2& a, const glm::uvec2& b) {
    //      if ((b - a).y == 0 && a.y == 0 && b.x == last_x) {
    //        const auto coord = glm::uvec2(last_x, 0);
    //        fun(coord, raster.value(coord.y, coord.x)); // raster is row / column
    //      }
    //    };
    //    check_br(a, b);
    //    check_br(b, c);
    //    check_br(c, a);
    //  }
    const auto last_y = raster.get_height() - 1;
    if (max.x == last_x && max.y == last_y) {
        const auto check_tr = [&](const glm::uvec2& a, const glm::uvec2& b) {
            if ((b - a).x == 0 && b.y == last_y && b.x == last_x) {
                const auto coord = glm::uvec2(last_x, last_y);
                fun(coord, raster.value(coord.y, coord.x)); // raster is row / column
            }
        };
        check_tr(a, b);
        check_tr(b, c);
        check_tr(c, a);
    }
}
}
#endif
