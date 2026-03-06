#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/topology.h"

std::vector<glm::uvec2> find_non_manifold_edges(const std::span<const glm::uvec3> &triangles) {
    const auto edges_to_triangles = create_edge_to_triangle_mapping_non_manifold(triangles);
    std::vector<glm::uvec2> non_manifold_edges;

    for (const auto &[edge, triangle_indices] : edges_to_triangles) {
        if (triangle_indices.size() > 2) {
            non_manifold_edges.push_back(edge);
        }
    }

    return non_manifold_edges;
}

bool is_manifold(const std::span<const glm::uvec3> triangles) {
    const auto edge_to_faces = create_edge_to_triangle_mapping_non_manifold(triangles);
    for (const auto &[_edge, faces] : edge_to_faces) {
        if (faces.size() > 2u) {
            return false;
        }
    }
    return true;
}
