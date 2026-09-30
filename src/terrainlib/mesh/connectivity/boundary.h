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
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_vertices(const mesh::Simple_<n_dims, T> &mesh);
std::unordered_set<uint32_t> find_boundary_vertices(const std::span<const glm::uvec3> triangles);
void find_boundary_vertices(const std::span<const glm::uvec3> triangles, std::unordered_set<uint32_t> &boundary);

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_edges(const mesh::Simple_<n_dims, T> &mesh);
std::unordered_set<glm::uvec2> find_boundary_edges(const std::span<const glm::uvec3> triangles);
void find_boundary_edges(const std::span<const glm::uvec3> triangles, std::unordered_set<glm::uvec2> &boundary);

template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_triangles(const mesh::Simple_<n_dims, T> &mesh);
std::unordered_set<uint32_t> find_boundary_triangles(const std::span<const glm::uvec3> triangles);
void find_boundary_triangles(const std::span<const glm::uvec3> triangles, std::unordered_set<uint32_t> &boundary);

template <glm::length_t n_dims, typename T>
std::vector<uint8_t> build_boundary_vertex_mask(const mesh::Simple_<n_dims, T> &mesh);
std::vector<uint8_t> build_boundary_vertex_mask(const std::span<const glm::uvec3> triangles, const size_t vertex_count);
template <typename MaskT = uint8_t>
void build_boundary_vertex_mask(const std::span<const glm::uvec3> triangles, const size_t vertex_count, std::vector<MaskT> &boundary, const MaskT on_boundary = MaskT{1}, const MaskT not_on_boundary = MaskT{0});

template <glm::length_t n_dims, typename T>
std::vector<uint8_t> build_boundary_triangle_mask(const mesh::Simple_<n_dims, T> &mesh);
std::vector<uint8_t> build_boundary_triangle_mask(const std::span<const glm::uvec3> triangles);
template <typename MaskT = uint8_t>
void build_boundary_triangle_mask(const std::span<const glm::uvec3> triangles, std::vector<MaskT> &boundary, const MaskT on_boundary = MaskT{1}, const MaskT not_on_boundary = MaskT{0});

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries(const mesh::Simple_<n_dims, T> &mesh);
std::vector<std::vector<uint32_t>> find_boundaries(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries_non_manifold(const mesh::Simple_<n_dims, T> &mesh);
std::vector<std::vector<uint32_t>> find_boundaries_non_manifold(const std::span<const glm::uvec3> triangles);

}

#include "boundary.inl"
