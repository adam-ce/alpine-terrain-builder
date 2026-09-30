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

#include <vector>
#include <span>

#include <glm/common.hpp>
#include <igl/bfs_orient.h>

#include "mesh/igl/convert.h"
#include "mesh/connectivity/adjacency.h"

#include "mesh/connectivity/manifold.h"
#include "mesh/igl/manifold.h"

namespace mesh {

namespace detail {
void orient_triangles_core(
    const std::span<const glm::uvec3> triangles_in,
    const std::span<glm::uvec3> triangles_out) {
    auto F = convert_triangles(triangles_in);
    Eigen::MatrixX3i FF;
    Eigen::VectorXi C;
    ::igl::bfs_orient(F, FF, C);
    to_stl_glm_span(FF, triangles_out);
    DEBUG_ASSERT(is_consistently_oriented(triangles_out));
}
}

void orient_triangles_inplace(const std::span<glm::uvec3> triangles) {
    detail::orient_triangles_core(triangles, triangles);
}

std::vector<glm::uvec3> orient_triangles(const std::span<glm::uvec3> triangles) {
    std::vector<glm::uvec3> reoriented(triangles.size());
    detail::orient_triangles_core(triangles, reoriented);
    return reoriented;
}

} // namespace mesh
