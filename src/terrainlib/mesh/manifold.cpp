#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/utils.h"

std::vector<glm::uvec2> find_non_manifold_edges(const std::span<const glm::uvec3> &triangles) {
    const auto edges_to_triangles = create_edge_to_triangle_mapping_non_manifold2(triangles);
    std::vector<glm::uvec2> non_manifold_edges;

    for (const auto &[edge, triangle_indices] : edges_to_triangles) {
        if (triangle_indices.size() > 2) {
            non_manifold_edges.push_back(edge);
        }
    }

    return non_manifold_edges;
}
