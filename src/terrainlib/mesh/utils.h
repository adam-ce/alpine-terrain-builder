#pragma once

#include <optional>
#include <span>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "FixedVector.h"
#include "HybridVector.h"
#include "mesh/WindingOrder.h"

// TODO: put in mesh namespace
// TODO: make all methods work with SimpleMesh_<n_dims, T>

template <typename T>
concept TriangleContainer = requires(T c) {
    { c.size() } -> std::convertible_to<size_t>;
    { c[0] } -> std::convertible_to<glm::uvec3>;
    { c[1] } -> std::convertible_to<glm::uvec3>;
    { c[2] } -> std::convertible_to<glm::uvec3>;
};

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
std::vector<uint32_t> find_isolated_vertices(const SimpleMesh_<n_dims, T> &mesh);
size_t remove_isolated_vertices(SimpleMesh & mesh);
size_t remove_triangles_of_negligible_size(SimpleMesh &mesh, const double threshold_percentage_of_average = 0.001);

bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, bool ignore_orientation = true);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    bool ignore_orientation = true);
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
void remove_duplicate_triangles(mesh::Simple_<3, T> &mesh, bool ignore_orientation = true);
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions,
    bool ignore_orientation = true);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::uvec3> &positions,
    bool ignore_orientation);
void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3>& triangles);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::uvec3> &positions);

std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_index_mapping(const SimpleMesh &mesh);
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_index_mapping(const std::span<const glm::uvec3> triangles);
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_index_mapping_non_manifold(const SimpleMesh &mesh);
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_index_mapping_non_manifold(const std::span<const glm::uvec3> triangles);
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_index_mapping_non_manifold2(const SimpleMesh &mesh);
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_index_mapping_non_manifold2(const std::span<const glm::uvec3> triangles);

std::vector<uint32_t> count_vertex_adjacent_triangles(const SimpleMesh &mesh);
std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles);

std::vector<glm::uvec2> find_non_manifold_edges(const SimpleMesh &mesh);
std::vector<uint32_t> find_single_non_manifold_triangle_indices(const SimpleMesh &mesh);
void remove_single_non_manifold_triangles(SimpleMesh & mesh);

void normalize_face_index_rotation(std::span<uint32_t> face, bool keep_orientation = true);
void normalize_edge_inplace(glm::uvec2 & edge);
glm::uvec2 normalize_edge(glm::uvec2 edge);
void normalize_triangle_inplace(glm::uvec3 & triangle, bool keep_orientation = true);
glm::uvec3 normalize_triangle(glm::uvec3 triangle, bool keep_orientation = true);
void normalize_quad_inplace(glm::uvec4 & quad, bool keep_orientation = true);
glm::uvec4 normalize_quad(glm::uvec4 quad, bool keep_orientation = true);

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(SimpleMesh_<n_dims, T> & mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);

void reindex_mesh(SimpleMesh & mesh);
SimpleMesh reindex_mesh(const SimpleMesh &mesh);

void flip_triangle_orientation(glm::uvec3 & triangle);
void flip_triangle_orientations(std::vector<glm::uvec3> & triangles);
template <glm::length_t n_dims, typename T>
void flip_orientation(SimpleMesh_<n_dims, T> & mesh);

template <glm::length_t n_dims, typename T, typename Func>
std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> get_edges(const SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh, std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> &boundary);

bool is_degenerate(const glm::uvec3 &triangle);

template <typename T>
WindingOrder get_winding_order(const glm::vec<2, T> &a,
                                const glm::vec<2, T> &b,
                                const glm::vec<2, T> &c);
template <typename T>
WindingOrder get_winding_order(const glm::vec<2, uint32_t> &triangle,
                                const std::span<const glm::vec<2, T>> positions);

template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                                const std::span<const glm::vec<3, T>> positions,
                                const bool normalize = true);

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                                const glm::vec<3, T> &b,
                                const glm::vec<3, T> &c,
                                const bool normalize = true);

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F &&func, const bool normalize);
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func, const bool normalize);

void remove_degenerate_triangles(std::vector<glm::uvec3> &triangles);

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_edges(mesh::Simple_<n_dims, T> & mesh);
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> & positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> & positions,
    std::vector<glm::vec<2, Uv>> & uvs);

template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> & mesh);
template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3>& triangles,
    std::vector<glm::vec<n_dims, Position>> & positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void make_manifold(
    std::vector<glm::uvec3>& triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);

#include "utils.inl"
