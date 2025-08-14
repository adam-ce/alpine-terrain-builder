#pragma once

#include "mesh/SimpleMesh.h"

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
