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

#pragma once

#include <span>
#include <vector>

#include <glm/common.hpp>
#include <libassert/assert.hpp>

#include "enumerate.h"
#include "mesh/SimpleMesh.h"

namespace mesh::igl {

bool is_manifold(const std::span<const glm::uvec3> triangles);
bool is_edge_manifold(const std::span<const glm::uvec3> triangles);
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles);

struct TrianglesAndIndexMap {
    std::vector<glm::uvec3> triangles;
    std::vector<uint32_t> backwards;
};
TrianglesAndIndexMap make_manifold(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions) {
    const auto [new_triangles, backwards] = make_manifold(triangles);
    triangles = new_triangles;
    
    const size_t old_position_count = positions.size();
    positions.reserve(backwards.size());
    for (const auto [i, original_index] : enumerate(backwards)) {
        if (i < old_position_count) {
            DEBUG_ASSERT(original_index == i);
        } else {
            positions.push_back(positions[original_index]);
        }
    }
}
template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> &mesh) {
    make_manifold(mesh.triangles, mesh.positions);
}

} // namespace mesh
