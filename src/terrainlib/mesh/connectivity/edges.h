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
#include <unordered_set>
#include <vector>

#include <glm/common.hpp>

#include "mesh/TriangleContainer.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
std::unordered_set<glm::uvec2> get_edges(const Triangles &triangles);

template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> get_halfedges(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> get_halfedges(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
std::vector<glm::uvec2> get_halfedges(const Triangles &triangles);

template <glm::length_t n_dims, typename T>
uint32_t compute_edge_count(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
uint32_t compute_edge_count(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
uint32_t compute_edge_count(const Triangles &triangles);

template <glm::length_t n_dims, typename T>
uint32_t compute_halfedge_count(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
uint32_t compute_halfedge_count(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
uint32_t compute_halfedge_count(const Triangles &triangles);

template <glm::length_t n_dims, typename T, typename F>
void for_each_halfedge(const mesh::Simple_<n_dims, T> &mesh, F &&func, const bool normalize = false);
template <glm::length_t n_dims, typename T, typename F>
void for_each_halfedge(const mesh::View_<n_dims, T> &mesh, F &&func, const bool normalize = false);
template <TriangleContainer Triangles, typename F>
void for_each_halfedge(const Triangles &triangles, F &&func, const bool normalize = false);

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F &&func);
template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::View_<n_dims, T> &mesh, F &&func);
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func);

} // namespace mesh

#include "edges.inl"