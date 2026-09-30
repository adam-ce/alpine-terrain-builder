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
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/VertexMap.h"

namespace mesh {

struct TrianglesAndMap {
    std::vector<glm::uvec3> triangles;
    VertexMap remap;
};

template <glm::length_t n_dims, typename T>
struct MeshAndMap {
    mesh::Simple_<n_dims, T> mesh;
    VertexMap remap;
};

// Builds a compact reindexing map based on first vertex encounter order
VertexMap create_reindex_map(std::span<const glm::uvec3> triangles);

void reindex_inplace(std::span<glm::uvec3> triangles);
VertexMap reindex_inplace_with_map(std::span<glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>

void reindex_inplace(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
VertexMap reindex_inplace_with_map(mesh::Simple_<n_dims, T> &mesh);

std::vector<glm::uvec3> reindex(std::span<const glm::uvec3> triangles);
TrianglesAndMap reindex_with_map(std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::View_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
MeshAndMap<n_dims, T> reindex_with_map(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
MeshAndMap<n_dims, T> reindex_with_map(const mesh::Simple_<n_dims, T> &mesh);


} // namespace mesh

#include "reindex.inl"