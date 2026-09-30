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

#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
T compute_surface_area(const std::span<const glm::uvec3> triangles, const std::span<const glm::vec<n_dims, T>> positions) {
    T total_area = static_cast<T>(0);
    for (const glm::uvec3& triangle : triangles) {
        const T triangle_area = geometry::compute_triangle_area<n_dims, T>(triangle, positions);
        total_area += triangle_area;
    }
    return total_area;
}
template <glm::length_t n_dims, typename T>
T compute_surface_area(const std::vector<glm::uvec3>& triangles, const std::vector<glm::vec<n_dims, T>>& positions) {
    return compute_surface_area<n_dims, T>(std::span<const glm::uvec3>(triangles), std::span<const glm::vec<n_dims, T>>(positions));
}
template <glm::length_t n_dims, typename T>
T compute_surface_area(const mesh::View_<n_dims, T>& mesh) {
    return compute_surface_area<n_dims, T>(mesh.triangles, mesh.positions);
}
template <glm::length_t n_dims, typename T>
T compute_surface_area(const mesh::Simple_<n_dims, T>& mesh) {
    return compute_surface_area<n_dims, T>(mesh.triangles, mesh.positions);
}

} // namespace mesh