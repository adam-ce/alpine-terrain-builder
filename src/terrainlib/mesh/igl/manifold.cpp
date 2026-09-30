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
#include <igl/is_edge_manifold.h>
#include <igl/is_vertex_manifold.h>
#include <igl/split_nonmanifold.h>

#include "mesh/igl/convert.h"

namespace mesh::igl {

bool is_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    return ::igl::is_edge_manifold(F) && ::igl::is_vertex_manifold(F);
}
bool is_edge_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    return ::igl::is_edge_manifold(F);
}
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    return ::igl::is_vertex_manifold(F);
}

TrianglesAndIndexMap make_manifold(const std::span<const glm::uvec3> triangles) {
    auto F = convert_triangles(triangles);
    Eigen::MatrixX3i SF;
    Eigen::VectorXi SVI;
    ::igl::split_nonmanifold(F, SF, SVI);
    return {
        convert_triangles(SF),
        to_stl_vector<decltype(SVI), uint32_t>(SVI)
    };
}

} // namespace mesh
