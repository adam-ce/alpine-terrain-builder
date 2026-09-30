/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <algorithm>

#include <glm/glm.hpp>

#include "mesh/normalize.h"

namespace mesh {

inline constexpr bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    // First, compare by x
    if (t1.x != t2.x) {
        return t1.x < t2.x;
    }

    // If x is equal, compare by y
    if (t1.y != t2.y) {
        return t1.y < t2.y;
    }

    // If x and y are equal, compare by z
    return t1.z < t2.z;
}

inline bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return compare_triangles(normalize_triangle(t1), normalize_triangle(t2));
}

inline bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return normalize_triangle(t1) == normalize_triangle(t2);
}
inline bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return std::is_permutation(&t1.x, &t1.z + 1, &t2.x);
}

}
