#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "FixedVector.h"
#include "HybridVector.h"
#include "TriangleContainer.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

bool is_degenerate(const glm::uvec3 &triangle);

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(mesh::Simple_<n_dims, T> &mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>
void sort_triangles(mesh::Simple_<n_dims, T> &mesh);
void sort_triangles(std::span<glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
void flip_orientation(mesh::Simple_<n_dims, T> &mesh);
constexpr void flip_triangle_orientation(glm::uvec3 &triangle);
void flip_triangle_orientations(std::span<glm::uvec3> triangles);

uint32_t compute_vertex_count(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::Simple_<n_dims, T> &mesh, const bool normalize = false);
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::View_<n_dims, T> &mesh, const bool normalize = false);
template <TriangleContainer Triangles, typename F>
std::unordered_set<glm::uvec2> get_edges(const Triangles &triangles, const bool normalize = false);

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F &&func, const bool normalize);
template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::View_<n_dims, T> &mesh, F &&func, const bool normalize);
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func, const bool normalize);

constexpr glm::uvec2 other_vertices_in_triangle(const glm::uvec3 &triangle, const uint32_t vertex);

constexpr glm::uvec3 change_vertex(const glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex);
constexpr void change_vertex_inplace(glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex);

void normalize_face_index_rotation(const std::span<uint32_t> face, const bool keep_orientation);

constexpr glm::uvec2 normalize_edge(glm::uvec2 edge);
constexpr void normalize_edge_inplace(glm::uvec2 &edge);

glm::uvec3 normalize_triangle(glm::uvec3 triangle, const bool keep_orientation = true);
void normalize_triangle_inplace(glm::uvec3 &triangle, const bool keep_orientation = true);
void normalize_triangles_inplace(std::span<glm::uvec3> triangles, const bool keep_orientation = true);
void normalize_triangles_inplace(std::vector<glm::uvec3>& triangles, const bool keep_orientation = true);

glm::uvec4 normalize_quad(glm::uvec4 quad, const bool keep_orientation = true);
void normalize_quad_inplace(glm::uvec4 &quad, const bool keep_orientation = true);

constexpr bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

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
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const std::span<const glm::uvec3> triangles, const size_t vertex_count);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::View_<n_dims, T> &mesh);
std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles, const size_t vertex_count);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const mesh::View_<n_dims, T> &mesh);
std::vector<uint32_t> find_isolated_vertices(const std::span<const glm::uvec3> triangles, const size_t vertex_count);

}

#include "topology.inl"
