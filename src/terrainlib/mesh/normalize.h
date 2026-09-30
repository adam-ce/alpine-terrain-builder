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

namespace mesh {

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(mesh::Simple_<n_dims, T> &mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>
void sort_triangles(mesh::Simple_<n_dims, T> &mesh);
void sort_triangles(std::span<glm::uvec3> triangles);

void normalize_face_index_rotation(const std::span<uint32_t> face, const bool keep_orientation);

constexpr glm::uvec2 normalize_edge(glm::uvec2 edge);
constexpr void normalize_edge_inplace(glm::uvec2 &edge);

glm::uvec3 normalize_triangle(glm::uvec3 triangle, const bool keep_orientation = true);
void normalize_triangle_inplace(glm::uvec3 &triangle, const bool keep_orientation = true);
void normalize_triangles_inplace(std::span<glm::uvec3> triangles, const bool keep_orientation = true);
void normalize_triangles_inplace(std::vector<glm::uvec3>& triangles, const bool keep_orientation = true);

glm::uvec4 normalize_quad(glm::uvec4 quad, const bool keep_orientation = true);
void normalize_quad_inplace(glm::uvec4 &quad, const bool keep_orientation = true);

}

#include "normalize.inl"
