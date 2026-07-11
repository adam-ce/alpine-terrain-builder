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
