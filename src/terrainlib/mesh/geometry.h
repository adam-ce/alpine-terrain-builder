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
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "geometry/geometry.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
T compute_surface_area(const std::span<const glm::uvec3> triangles, const std::span<const glm::vec<n_dims, T>> positions);
template <glm::length_t n_dims, typename T>
T compute_surface_area(const std::vector<glm::uvec3> &triangles, const std::vector<glm::vec<n_dims, T>> &positions);
template <glm::length_t n_dims, typename T>
T compute_surface_area(const mesh::View_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
T compute_surface_area(const mesh::Simple_<n_dims, T> &mesh);

std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size = 1000);

std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh);
std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh);

std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh);
std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh);

} // namespace mesh

#include "geometry.inl"