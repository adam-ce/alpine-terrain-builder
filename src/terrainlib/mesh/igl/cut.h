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
#include <ranges>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "enumerate.h"

namespace mesh {

template <std::ranges::range Cuts>
std::vector<glm::bvec3> cuts_to_edge_mask(
    const Cuts cuts,
    const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut(
    mesh::Simple_<n_dims, T> &mesh,
    const std::span<const glm::bvec3> edge_cut_mask);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    const std::span<const glm::bvec3> edge_cut_mask);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    std::vector<glm::vec<2, T>> &uvs,
    const std::span<const glm::bvec3> edge_cut_mask);
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    const std::span<const glm::bvec3> edge_cut_mask);
template <typename Reserve, typename Duplicate>
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    const std::span<const glm::bvec3> edge_cut_mask,
    Reserve &&reserve,
    Duplicate &&duplicate);

} // namespace mesh

#include "cut.inl"
