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

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::View_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::Simple_<3, T> &mesh);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::View_<3, T> &mesh);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::Simple_<3, T> &mesh);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::View_<3, T> &mesh);
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::vector<glm::uvec3> &triangles);
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles);

template <typename T>
void remove_duplicate_triangles(mesh::Simple_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions);
template <typename T>
void remove_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh);
void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3> &triangles);

template <glm::length_t n_dims, typename T>
size_t remove_isolated_vertices(SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T, typename Size = float>
size_t remove_triangles_of_negligible_size(
    SimpleMesh_<n_dims, T> &mesh,
    const Size threshold_percentage_of_average);

void remove_degenerate_triangles(std::vector<glm::uvec3> &triangles);

}

#include "cleanup.inl"
