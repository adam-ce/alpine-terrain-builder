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

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                              const glm::vec<3, T> &b,
                              const glm::vec<3, T> &c,
                              const bool normalize = true);
template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                              const std::span<const glm::vec<3, T>> positions,
                              const bool normalize = true);

std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size = 1000);

std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh);
std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh);

std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh);
std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh);

#include "geometry.inl"
