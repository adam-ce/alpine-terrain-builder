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
#include <span>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/VertexMap.h"
#include "mesh/View.h"

namespace mesh {

void find_cut_to_disk(const std::span<const glm::uvec3> &triangles, std::vector<std::vector<uint32_t>> &cuts);
inline std::vector<std::vector<uint32_t>> find_cut_to_disk(const std::span<const glm::uvec3> &triangles);
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_cut_to_disk(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_cut_to_disk(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    std::vector<glm::vec<2, T>> &uvs);
std::vector<uint32_t> cut_to_disk(std::span<glm::uvec3> triangles);
template <typename Reserve, typename Duplicate>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    Reserve &&reserve,
    Duplicate &&duplicate);

} // namespace mesh

#include "cut_to_disk.inl"
