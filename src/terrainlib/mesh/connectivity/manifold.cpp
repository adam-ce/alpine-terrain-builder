#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "containers/UnionFind.h"
#include "mesh/SimpleMesh.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/connectivity/adjacency.h"
#include "mesh/connectivity/vertex_index_range.h"

namespace mesh {
    
std::vector<glm::uvec2> find_non_manifold_edges(const std::span<const glm::uvec3> triangles) {
    const auto edges_to_triangles = create_edge_to_triangle_mapping_non_manifold(triangles);
    std::vector<glm::uvec2> non_manifold_edges;

    for (const auto &[edge, triangle_indices] : edges_to_triangles) {
        if (triangle_indices.size() > 2) {
            non_manifold_edges.push_back(edge);
        }
    }

    return non_manifold_edges;
}

bool is_manifold(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));
    return is_edge_manifold(triangles) && is_vertex_manifold(triangles, vertex_count);
}

bool is_manifold(const std::span<const glm::uvec3> triangles) {
    const uint32_t vertex_count = vertex_buffer_size(triangles);
    return is_manifold(triangles, vertex_count);
}

bool is_edge_manifold(const std::span<const glm::uvec3> triangles) {
    const auto edge_to_faces = create_edge_to_triangle_mapping_non_manifold(triangles);
    for (const auto &[_edge, faces] : edge_to_faces) {
        if (faces.size() > 2u) {
            return false;
        }
    }
    return true;
}

bool is_vertex_manifold(const std::span<const glm::uvec3> triangles) {
    const uint32_t vertex_count = vertex_buffer_size(triangles);
    return is_vertex_manifold(triangles, vertex_count);
}
bool is_vertex_manifold(const std::span<const glm::uvec3> triangles, const uint32_t vertex_count) {
    if (triangles.empty()) {
        return true;
    }
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));

    const auto vertex_to_triangles = create_vertex_to_triangle_mapping(triangles, vertex_count);

    std::unordered_map<uint32_t, std::vector<uint32_t>> edge_groups;
    UnionFind_<true, uint32_t, uint32_t> union_find;

    const auto is_vertex_manifold_at = [&](const uint32_t vertex_index) {
        const auto &incident_triangles = vertex_to_triangles[vertex_index];
        const uint32_t incident_count = (uint32_t)incident_triangles.size();
        if (incident_count <= 1) {
            return true;
        }

        edge_groups.clear();
        edge_groups.reserve(incident_count * 2);

        // Group incident triangles by edge (vertex, neighbor)
        for (uint32_t local_index = 0; local_index < incident_count; local_index++) {
            const glm::uvec3 &triangle = triangles[incident_triangles[local_index]];
            const glm::uvec2 neighbors = other_vertices_in_triangle(triangle, vertex_index);

            edge_groups[neighbors.x].push_back(local_index);
            edge_groups[neighbors.y].push_back(local_index);
        }

        union_find.reset(incident_count);
        for (const auto &[_, group] : edge_groups) {
            for (uint32_t i = 1; i < group.size(); i++) {
                union_find.make_union(group[0], group[i]);
            }
        }

        return union_find.is_joint();
    };

    for (uint32_t vertex = 0; vertex < vertex_count; vertex++) {
        if (!is_vertex_manifold_at(vertex)) {
            return false;
        }
    }
    return true;
}

} // namespace mesh
