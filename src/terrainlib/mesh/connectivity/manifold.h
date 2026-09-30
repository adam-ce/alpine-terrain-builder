/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

namespace mesh {

std::vector<glm::uvec2> find_non_manifold_edges(const std::span<const glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> find_non_manifold_edges(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> find_non_manifold_edges(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_edges(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);
template <typename Duplicate>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    Duplicate &&duplicate_vertex);

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_vertices(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);
template <typename Duplicate>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    uint32_t vertex_count,
    Duplicate &&duplicate_vertex);

template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);
template <typename Duplicate>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    const uint32_t vertex_count,
    Duplicate &&duplicate_vertex);

template <glm::length_t n_dims, typename T>
bool is_manifold(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
bool is_manifold(const mesh::View_<n_dims, T> &mesh);
bool is_manifold(const std::span<const glm::uvec3> triangles);
bool is_manifold(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count);

template <glm::length_t n_dims, typename T>
bool is_edge_manifold(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
bool is_edge_manifold(const mesh::View_<n_dims, T> &mesh);
bool is_edge_manifold(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
bool is_vertex_manifold(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
bool is_vertex_manifold(const mesh::View_<n_dims, T> &mesh);
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles);
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count);
}

#include "manifold.inl"
