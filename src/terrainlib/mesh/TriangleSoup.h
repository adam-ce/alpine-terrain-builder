/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include <vector>
#include <array>
#include <span>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"

template <glm::length_t n_dims, typename T>
using TriangleSoup_ = std::vector<std::array<glm::vec<n_dims, T>, 3>>;
using TriangleSoup = TriangleSoup_<3, double>;

template <glm::length_t n_dims, typename T>
TriangleSoup_<n_dims, T> to_triangle_soup(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void sort_triangle_soup(TriangleSoup_<n_dims, T> &soup);
template <glm::length_t n_dims, typename T>
TriangleSoup_<n_dims, T> to_sorted_triangle_soup(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<TriangleSoup_<n_dims, T>> to_sorted_triangle_soups(const std::span<const mesh::Simple_<n_dims, T>> meshes);

#include "TriangleSoup.inl"
