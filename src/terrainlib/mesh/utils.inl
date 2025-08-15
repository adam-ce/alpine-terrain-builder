#pragma once

#include "mesh/SimpleMesh.h"

namespace {
template <typename MeshRange>
auto calculate_bounds_range(const MeshRange &meshes) {
    using Mesh = std::unwrap_reference_t<std::ranges::range_value_t<MeshRange>>;
    using Vec = std::remove_cvref_t<decltype(std::declval<Mesh>().positions[0])>;
    using T = typename Vec::value_type;
    constexpr glm::length_t n_dims = Vec::length();

    radix::geometry::Aabb<n_dims, T> bounds;
    constexpr T inf = std::numeric_limits<T>::infinity();
    bounds.min = Vec(+inf);
    bounds.max = Vec(-inf);

    for (const auto &mesh_ref : meshes) {
        const Mesh &mesh = mesh_ref;
        for (const auto &position : mesh.positions) {
            bounds.expand_by(position);
        }
    }

    return bounds;
}
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh) {
    return calculate_bounds_range(std::array{std::cref(mesh)});
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes) {
    return calculate_bounds_range(meshes);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes) {
    return calculate_bounds_range(meshes);
}

template <glm::length_t n_dims, typename T>
std::vector<size_t> find_isolated_vertices(const SimpleMesh_<n_dims, T> &mesh) {
    std::vector<bool> connected;
    connected.resize(mesh.vertex_count());
    std::fill(connected.begin(), connected.end(), false);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            connected[triangle[k]] = true;
        }
    }

    std::vector<size_t> isolated;
    for (size_t i = 0; i < mesh.vertex_count(); i++) {
        if (!connected[i]) {
            isolated.push_back(i);
        }
    }

    return isolated;
}

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(SimpleMesh_<n_dims, T> &mesh) {
    sort_and_normalize_triangles(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
void flip_orientation(SimpleMesh_<n_dims, T> &mesh) {
    flip_triangle_orientations(mesh.triangles);
}
