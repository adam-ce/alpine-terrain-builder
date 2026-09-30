/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <cstdint>
#include <utility>

#include "containers/FixedVector.h"
#include "mesh/geometry.h"
#include "mesh/WindingOrder.h"
#include "polygon/clip.h"
#include "range_utils.h"

namespace polygon {

namespace {
// Clipping a triangle by three half spaces adds at most one corner per half space.
constexpr uint32_t max_corners = 6;
using Corners = FixedVector<glm::dvec2, max_corners>;
using Edge2d = radix::geometry::Edge<2, double>;

// Sutherland-Hodgman against one half space. Corners on the edge count as inside.
void clip_to_half_space(const Corners &corners, const Edge2d &edge, Corners &out) {
    out.clear();
    if (corners.empty()) {
        return;
    }

    glm::dvec2 previous = corners.back();
    double previous_side = geometry::orient(edge[0], edge[1], previous); // sign == side

    for (const glm::dvec2 &current : corners) {
        const double current_side = geometry::orient(edge[0], edge[1], current); // sign == side

        // Only a strict crossing needs a new corner; one lying on the edge is already its own.
        const bool crosses = (previous_side > 0.0 && current_side < 0.0) ||
                             (previous_side < 0.0 && current_side > 0.0);
        if (crosses) {
            const double t = previous_side / (previous_side - current_side);
            out.push_back(glm::mix(previous, current, t));
        }
        if (current_side >= 0.0) {
            out.push_back(current);
        }

        previous = current;
        previous_side = current_side;
    }
}

} // namespace

void clip_triangle(const Triangle2d &subject, const Triangle2d &clip, std::vector<Triangle2d> &out) {
    const Triangle2d bounds = as_counter_clockwise(clip);

    Corners corners;
    corners.append_range(subject);

    // Clip against each edge half space of the bounds
    Corners clipped;
    Corners *current = &corners;
    Corners *next = &clipped;
    for (const uint8_t corner : range<uint8_t>(3)) {
        clip_to_half_space(*current, Edge2d{bounds[corner], bounds[(corner + 1) % 3]}, *next);
        std::swap(current, next);
    }

    // Corners on a clip edge are kept by both sides, so the fan can hold collinear runs.
    for (uint32_t corner = 2; corner < current->size(); corner++) {
        const Triangle2d piece = {(*current)[0], (*current)[corner - 1], (*current)[corner]};
        if (!geometry::is_empty_triangle(piece)) {
            out.push_back(piece);
        }
    }
}

} // namespace polygon
