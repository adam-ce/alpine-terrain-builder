#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/topology.h"

namespace mesh {

void sort_and_normalize_triangles(std::span<glm::uvec3> triangles) {
    // sort vertices in triangles
    for (glm::uvec3 &triangle : triangles) {
        triangle = normalize_triangle(triangle);
    }

    // sort triangle vector
    std::sort(triangles.begin(), triangles.end(), compare_triangles);
}

namespace {
template <typename IndexContainer>
std::unordered_map<glm::uvec2, IndexContainer>
create_edge_to_triangle_mapping_impl(const std::span<const glm::uvec3> triangles) {
    std::unordered_map<glm::uvec2, IndexContainer> edges_to_triangles;

    for_each_edge(triangles, [&](const glm::uvec2 &edge, const size_t triangle_index) {
        auto result = edges_to_triangles.try_emplace(edge, IndexContainer{}).first;
        result->second.push_back(triangle_index); }, true);

#ifndef NDEBUG
    // Validate the mapping
    for (const auto &[edge, triangle_indices] : edges_to_triangles) {
        DEBUG_ASSERT(!triangle_indices.empty(), "Each edge must have at least one triangle");

        for (const uint32_t triangle_index : triangle_indices) {
            DEBUG_ASSERT(triangle_index < triangles.size(), "Triangle index must be valid");

            const glm::uvec3 &triangle = triangles[triangle_index];

            const bool first_vertex_found_in_triangle =
                (triangle[0] == edge.x || triangle[1] == edge.x || triangle[2] == edge.x);

            const bool second_vertex_found_in_triangle =
                (triangle[0] == edge.y || triangle[1] == edge.y || triangle[2] == edge.y);

            DEBUG_ASSERT(first_vertex_found_in_triangle && second_vertex_found_in_triangle, "Both vertices of the edge exist in the triangle");
        }
    }
#endif

    return edges_to_triangles;
}
} // namespace

std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const std::span<const glm::uvec3> triangles) {
    return create_edge_to_triangle_mapping_impl<FixedVector<uint32_t, 2>>(triangles);
}
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const std::span<const glm::uvec3> triangles) {
    return create_edge_to_triangle_mapping_impl<HybridVector<uint32_t, 2>>(triangles);
}
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const std::span<const glm::uvec3> triangles) {
    return create_edge_to_triangle_mapping_impl<std::vector<uint32_t>>(triangles);
}

std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const std::span<const glm::uvec3> triangles, const size_t vertex_count) {
    std::vector<std::vector<uint32_t>> vertex_to_triangles(vertex_count);

    for (uint32_t triangle_index = 0; triangle_index < triangles.size(); triangle_index++) {
        const glm::uvec3 &triangle = triangles[triangle_index];

        vertex_to_triangles[triangle.x].push_back(triangle_index);
        vertex_to_triangles[triangle.y].push_back(triangle_index);
        vertex_to_triangles[triangle.z].push_back(triangle_index);
    }

    return vertex_to_triangles;
}

std::vector<uint32_t> count_vertex_adjacent_triangles(const std::span<const glm::uvec3> triangles, const size_t vertex_count) {
    std::vector<uint32_t> adjacent_triangle_count(vertex_count, 0);

    for (const glm::uvec3 &triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            adjacent_triangle_count[triangle[k]]++;
        }
    }

    return adjacent_triangle_count;
}

void flip_triangle_orientations(std::span<glm::uvec3> triangles) {
    for (auto &triangle : triangles) {
        flip_triangle_orientation(triangle);
    }
}

std::vector<uint32_t> find_isolated_vertices(const std::span<const glm::uvec3> triangles, const size_t vertex_count) {
    std::vector<bool> connected;
    connected.resize(vertex_count, false);
    for (const glm::uvec3 &triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            connected[triangle[k]] = true;
        }
    }

    std::vector<uint32_t> isolated;
    for (uint32_t i = 0; i < vertex_count; i++) {
        if (!connected[i]) {
            isolated.push_back(i);
        }
    }

    return isolated;
}

}
