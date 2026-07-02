#pragma once

#include <span>
#include <vector>

#include <glm/common.hpp>
#include <libassert/assert.hpp>

#include "enumerate.h"
#include "mesh/SimpleMesh.h"

namespace mesh::igl {

bool is_manifold(const std::span<const glm::uvec3> triangles);
bool is_edge_manifold(const std::span<const glm::uvec3> triangles);
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles);

struct TrianglesAndIndexMap {
    std::vector<glm::uvec3> triangles;
    std::vector<uint32_t> backwards;
};
TrianglesAndIndexMap make_manifold(const std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions) {
    const auto [new_triangles, backwards] = make_manifold(triangles);
    triangles = new_triangles;
    
    const size_t old_position_count = positions.size();
    positions.resize(backwards.size());
    for (const auto [i, original_index] : enumerate(backwards)) {
        if (i < old_position_count) {
            DEBUG_ASSERT(original_index == i);
        } else {
            positions.push_back(positions[original_index]);
        }
    }
}
template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> &mesh) {
    make_manifold(mesh.triangles, mesh.positions);
}

} // namespace mesh
