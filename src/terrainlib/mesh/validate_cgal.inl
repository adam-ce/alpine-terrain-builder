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

#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <libassert/assert.hpp>

#include "build_config.h"
#include "log.h"
#include "mesh/validate_cgal.h"

namespace cgal {

template <typename Point>
inline void validate(const CGAL::Surface_mesh<Point> &mesh) {
    if constexpr (IS_DEBUG_BUILD) {
        DEBUG_ASSERT(mesh.is_valid());
        DEBUG_ASSERT(CGAL::is_triangle_mesh(mesh));
        DEBUG_ASSERT(CGAL::is_valid_polygon_mesh(mesh));
        DEBUG_ASSERT(!CGAL::Polygon_mesh_processing::does_self_intersect(mesh));
    } else {
        ALP_UNUSED(mesh);
    }
}
}
