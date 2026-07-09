#pragma once

#include "spatial_lookup/GridStorage.h"
#include "spatial_lookup/CellBased.h"

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
using Grid = CellBased<n_dims, Component, Value, GridStorage<n_dims, Component, Value>>;

template <typename Value>
using Grid2d = Grid<2, double, Value>;
template <typename Value>
using Grid3d = Grid<3, double, Value>;

}

/*
namespace {
radix::geometry::Aabb3d pad_bounds(const radix::geometry::Aabb3d &bounds, const double percentage) {
    const glm::dvec3 bounds_padding = bounds.size() * percentage;
    const radix::geometry::Aabb3d padded_bounds(bounds.min - bounds_padding, bounds.max + bounds_padding);
    return padded_bounds;
}

template <glm::length_t n_dims, typename Component, typename Value>
Grid<n_dims, Component, Value> _construct_grid_for_meshes(const radix::geometry::Aabb3d &bounds, const size_t vertex_count) {
    const radix::geometry::Aabb3d padded_bounds = pad_bounds(bounds, 0.01);

    const double max_extends = max_component(padded_bounds.size());
    const glm::dvec3 relative_extends = padded_bounds.size() / max_extends;
    const glm::uvec3 grid_divisions = glm::max(glm::uvec3(2 * std::cbrt(vertex_count) * relative_extends), glm::uvec3(1));
    Grid<n_dims, Component, Value> grid(padded_bounds.min, padded_bounds.size(), grid_divisions);

    return grid;
}
} // namespace

template <glm::length_t n_dims, typename Component, typename Value>
inline Grid<n_dims, Component, Value> construct_grid_for_mesh(const SimpleMesh_<n_dims, Component> &mesh) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(mesh);
    const size_t vertex_count = mesh.vertex_count();
    return _construct_grid_for_meshes<n_dims, Component, Value>(bounds, vertex_count);
}

template <glm::length_t n_dims, typename Component, typename Value>
inline Grid<n_dims, Component, Value> construct_grid_for_meshes(const std::span<const SimpleMesh_<n_dims, Component>> meshes) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(meshes);
    const size_t maximal_merged_mesh_size = std::transform_reduce(
        meshes.begin(), meshes.end(), 0,
        [](const size_t a, const size_t b) { return a + b; },
        [](const SimpleMesh_<n_dims, Component> &mesh) { return mesh.vertex_count(); });
    return _construct_grid_for_meshes<n_dims, Component, Value>(bounds, maximal_merged_mesh_size);
}
*/