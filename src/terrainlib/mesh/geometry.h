#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                              const glm::vec<3, T> &b,
                              const glm::vec<3, T> &c,
                              const bool normalize = true);
template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                              const std::span<const glm::vec<3, T>> positions,
                              const bool normalize = true);

template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);

template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);

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
