#pragma once

#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <radix/geometry.h>

radix::geometry::Aabb3d calculate_bounds(const SimpleMesh &mesh);
radix::geometry::Aabb3d calculate_bounds(std::span<const SimpleMesh> meshes);

std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size = 1000);
std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh);

std::vector<size_t> find_isolated_vertices(const SimpleMesh &mesh);
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

void sort_and_normalize_triangles(SimpleMesh& mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);

void validate_mesh(const SimpleMesh &mesh);
