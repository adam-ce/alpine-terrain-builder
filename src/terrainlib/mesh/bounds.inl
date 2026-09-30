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

#include <array>
#include <functional>
#include <limits>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "VecRange.h"
#include "mesh/bounds.h"

namespace detail {
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> empty_bounds() {
    radix::geometry::Aabb<n_dims, T> bounds;
    constexpr T inf = std::numeric_limits<T>::infinity();
    bounds.min = glm::vec<n_dims, T>(+inf);
    bounds.max = glm::vec<n_dims, T>(-inf);
    return bounds;
}
}

template <glm::length_t n_dims, typename T, VecRange<n_dims, T> Range>
void extend_bounds(radix::geometry::Aabb<n_dims, T> &bounds, const Range &points) {
    for (const auto &point : points) {
        bounds.expand_by(point);
    }
}

template <AnyVecRange Range>
auto calculate_bounds(const Range &points) {
    using T = range_scalar_t<Range>;
    constexpr glm::length_t n_dims = range_dims_v<Range>;
    
    radix::geometry::Aabb<n_dims, T> bounds = detail::empty_bounds<n_dims, T>();
    extend_bounds(bounds, points);
    return bounds;
}

namespace mesh {

namespace detail {
template <std::ranges::input_range MeshRange>
auto calculate_bounds_mesh_range(const MeshRange &meshes) {
    using Mesh = std::unwrap_reference_t<std::ranges::range_value_t<MeshRange>>;
    using Vec = std::remove_cvref_t<decltype(std::declval<Mesh>().positions[0])>;
    using T = typename Vec::value_type;
    constexpr glm::length_t n_dims = Vec::length();

    radix::geometry::Aabb<n_dims, T> bounds = ::detail::empty_bounds<n_dims, T>();

    for (const auto &mesh_ref : meshes) {
        const Mesh &mesh = mesh_ref;
        extend_bounds<n_dims, T>(bounds, mesh.positions);
    }

    return bounds;
}
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh) {
    return ::calculate_bounds(mesh.positions);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes) {
    return detail::calculate_bounds_mesh_range(meshes);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes) {
    return detail::calculate_bounds_mesh_range(meshes);
}


}
