#pragma once

#include <optional>
#include <span>
#include <unordered_set>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"

// TODO: put in mesh namespace
// TODO: make all methods work with SimpleMesh_<n_dims, T>

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes);
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes);

std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size = 1000);
std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh);
std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh);
std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh);
std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh);

template <glm::length_t n_dims, typename T>
std::vector<size_t> find_isolated_vertices(const SimpleMesh_<n_dims, T> &mesh);
size_t remove_isolated_vertices(SimpleMesh & mesh);
size_t remove_triangles_of_negligible_size(SimpleMesh & mesh, const double threshold_percentage_of_average = 0.001);

bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

template <typename Triangles>
inline auto find_duplicate_triangles(Triangles& triangles, bool ignore_orientation = true) {
    std::sort(std::begin(triangles), std::end(triangles), ignore_orientation ? compare_triangles_ignore_orientation : compare_triangles);
    return std::unique(std::begin(triangles), std::end(triangles), ignore_orientation ? compare_equality_triangles_ignore_orientation : compare_equality_triangles);
}
template <>
inline auto find_duplicate_triangles(SimpleMesh& mesh, bool ignore_orientation) {
    return find_duplicate_triangles(mesh.triangles, ignore_orientation);
}
void remove_duplicate_triangles(SimpleMesh& mesh, bool ignore_orientation = true);
void remove_duplicate_triangles(std::vector<glm::uvec3>& triangles, bool ignore_orientation = true);

std::unordered_map<glm::uvec2, std::vector<size_t>> create_edge_to_triangle_index_mapping(const SimpleMesh &mesh);
std::vector<size_t> count_vertex_adjacent_triangles(const SimpleMesh &mesh);

std::vector<glm::uvec2> find_non_manifold_edges(const SimpleMesh &mesh);
std::vector<size_t> find_single_non_manifold_triangle_indices(const SimpleMesh &mesh);
void remove_single_non_manifold_triangles(SimpleMesh& mesh);

void normalize_face_index_rotation(std::span<uint32_t> face);
void normalize_edge_inplace(glm::uvec2 &edge);
glm::uvec2 normalize_edge(glm::uvec2 edge);
void normalize_triangle_inplace(glm::uvec3 &triangle);
glm::uvec3 normalize_triangle(glm::uvec3 triangle);
void normalize_quad_inplace(glm::uvec4 &quad);
glm::uvec4 normalize_quad(glm::uvec4 quad);

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(SimpleMesh_<n_dims, T> &mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);

void reindex_mesh(SimpleMesh &mesh);
SimpleMesh reindex_mesh(const SimpleMesh &mesh);

void flip_triangle_orientation(glm::uvec3 &triangle);
void flip_triangle_orientations(std::vector<glm::uvec3> &triangles);
template <glm::length_t n_dims, typename T>
void flip_orientation(SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T, typename Func>
std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> get_edges(const SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh, std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> &boundary);

#include "utils.inl"
