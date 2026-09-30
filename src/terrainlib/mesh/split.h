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

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "containers/Vector2D.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
struct SplitByVertexResult {
    std::vector<mesh::Simple_<n_dims, T>> groups;
    std::vector<uint32_t> vertex_remap;
};

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group, const bool drop_mixed_triangles = true);
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex(const mesh::Simple_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group, const bool drop_mixed_triangles = true);

template <glm::length_t n_dims, typename T>
struct SplitByTriangleResult {
    std::vector<mesh::Simple_<n_dims, T>> groups;
    Vector2D<uint32_t> vertex_remap;
};

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group);
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle(const mesh::Simple_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group);

} // namespace mesh

#include "split.inl"
