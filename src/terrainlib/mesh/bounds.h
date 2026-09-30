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

#include <ranges>
#include <span>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "VecRange.h"

template <glm::length_t n_dims, typename T, VecRange<n_dims, T> Range>
void extend_bounds(radix::geometry::Aabb<n_dims, T> &bounds, const Range &points);
template <AnyVecRange Range>
auto calculate_bounds(const Range &points);

namespace mesh {
    
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes);

}

#include "bounds.inl"
