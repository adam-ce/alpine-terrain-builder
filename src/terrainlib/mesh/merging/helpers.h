#pragma once

#include <span>
#include <functional>

#include "mesh/SimpleMesh.h"
#include "mesh/utils.h"
#include "spatial_lookup/Grid.h"

namespace mesh::merging {

inline double estimate_merge_epsilon(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) {
    double total_length = 0.0;
    size_t total_triangle_count = 0;
    for (const auto &mesh_ref : meshes) {
        const SimpleMesh &mesh = mesh_ref.get();
        const auto avg_length_opt = estimate_average_edge_length(mesh);
        if (avg_length_opt) {
            const size_t num_triangles = mesh.triangles.size();
            total_length += avg_length_opt.value() * num_triangles;
            total_triangle_count += num_triangles;
        }
    }

    const double average_edge_length = total_length / total_triangle_count;
    const double distance_epsilon = average_edge_length / 100;
    return distance_epsilon;
}

namespace {
radix::geometry::Aabb3d pad_bounds(const radix::geometry::Aabb3d &bounds, const double percentage) {
    const glm::dvec3 bounds_padding = bounds.size() * percentage;
    const radix::geometry::Aabb3d padded_bounds(bounds.min - bounds_padding, bounds.max + bounds_padding);
    return padded_bounds;
}

template <typename T>
spatial_lookup::Grid3d<T> _construct_grid_for_meshes(const radix::geometry::Aabb3d &bounds, const size_t vertex_count) {
    const radix::geometry::Aabb3d padded_bounds = pad_bounds(bounds, 0.01);

    const double max_extends = glm::compMax(padded_bounds.size());
    const glm::dvec3 relative_extends = padded_bounds.size() / max_extends;
    const glm::uvec3 grid_divisions = glm::max(glm::uvec3(2 * std::cbrt(vertex_count) * relative_extends), glm::uvec3(1));
    spatial_lookup::Grid3d<T> grid(padded_bounds.min, padded_bounds.size(), grid_divisions);

    return grid;
}
}

template <typename T>
spatial_lookup::Grid3d<T> construct_grid_for_mesh(const SimpleMesh &mesh) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(mesh);
    const size_t vertex_count = mesh.vertex_count();
    return _construct_grid_for_meshes<T>(bounds, vertex_count);
}

template <typename T>
spatial_lookup::Grid3d<T> construct_grid_for_meshes(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(meshes);
    const size_t maximal_merged_mesh_size = std::transform_reduce(
        meshes.begin(), meshes.end(), 0, [](const size_t a, const size_t b) { return a + b; }, [](const SimpleMesh &mesh) { return mesh.vertex_count(); });
    return _construct_grid_for_meshes<T>(bounds, maximal_merged_mesh_size);
}

} // namespace mesh::merging
