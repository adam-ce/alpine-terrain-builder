#pragma once

#include <functional>
#include <span>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes);

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const glm::vec<n_dims, T>> positions);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::vector<glm::vec<n_dims, T>> &positions);
template <glm::length_t n_dims, typename T>
void extend_bounds(radix::geometry::Aabb<n_dims, T> &bounds, const std::span<const glm::vec<n_dims, T>> positions);

#include "mesh/bounds.inl"
