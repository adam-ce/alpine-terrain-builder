#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "FixedVector.h"
#include "HybridVector.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

constexpr bool is_degenerate(const glm::uvec3 &triangle);

template <glm::length_t n_dims, typename T>
constexpr void flip_orientation(mesh::Simple_<n_dims, T> &mesh);
constexpr void flip_triangle_orientation(glm::uvec3 &triangle);
constexpr void flip_triangle_orientations(std::span<glm::uvec3> triangles);

constexpr glm::uvec2 other_vertices_in_triangle(const glm::uvec3 &triangle, const uint32_t vertex);

constexpr glm::uvec3 change_vertex(const glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex, const bool allow_missing = false);
constexpr void change_vertex_inplace(glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex, const bool allow_missing = false);

template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const mesh::View_<n_dims, T> &mesh);
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const mesh::View_<n_dims, T> &mesh);
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const mesh::View_<n_dims, T> &mesh);
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const mesh::View_<n_dims, T> &mesh);
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const std::span<const glm::uvec3> triangles);
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const std::span<const glm::uvec3> triangles, const uint32_t max_vertex_index);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::View_<n_dims, T> &mesh);
std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles);
std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles, const uint32_t max_vertex_index);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const mesh::View_<n_dims, T> &mesh);
std::vector<uint32_t> find_isolated_vertices(const std::span<const glm::uvec3> triangles);
std::vector<uint32_t> find_isolated_vertices(const std::span<const glm::uvec3> triangles, const uint32_t max_vertex_index);

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const mesh::View_<n_dims, T> &mesh);
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const std::span<const glm::uvec3> triangles);
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const std::span<const glm::uvec3> triangles, const uint32_t max_vertex_index);

template <glm::length_t n_dims, typename T>
bool is_consistently_oriented(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
bool is_consistently_oriented(const mesh::View_<n_dims, T> &mesh);
bool is_consistently_oriented(const std::span<const glm::uvec3> triangles);

}

#include "topology.inl"
