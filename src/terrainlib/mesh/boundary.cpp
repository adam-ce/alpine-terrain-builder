#include <span>
#include <unordered_set>
#include <unordered_map>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/boundary.h"
#include "mesh/edges.h"
#include "mesh/manifold.h"
#include "vector_utils.h"

namespace mesh {

void find_boundary_edges(const std::span<const glm::uvec3> triangles, std::unordered_set<glm::uvec2>& boundary) {
    boundary.clear();
    for_each_halfedge(triangles, [&](const glm::uvec2 &edge) {
        auto it = boundary.find(glm::uvec2(edge.y, edge.x));
        if (it != boundary.end()) {
            // Edge already there -> shared egde -> remove it
            boundary.erase(it);
        } else {
            // Edge not present -> add it (but in correct order)
            boundary.insert(edge);
        }
    }, /* normalize */ false);
}

void find_boundary_triangles(const std::span<const glm::uvec3> triangles, std::unordered_set<uint32_t>& boundary_triangles) {
    const std::unordered_map<glm::uvec2, uint32_t> edge_to_triangle = detail::find_boundary_edges_and_triangles(triangles);

    boundary_triangles.clear();
    boundary_triangles.reserve(edge_to_triangle.size());
    for (const auto &[_, triangle_index] : edge_to_triangle) {
        boundary_triangles.insert(triangle_index);
    }
}

void find_boundary_vertices(const std::span<const glm::uvec3> triangles, std::unordered_set<uint32_t> &boundary_vertices) {
    const std::unordered_set<glm::uvec2> boundary_edges = find_boundary_edges(triangles);

    boundary_vertices.clear();
    for (const glm::uvec2& edge : boundary_edges) {
        boundary_vertices.insert(edge.x);
        boundary_vertices.insert(edge.y);
    }
}

std::vector<std::vector<uint32_t>> find_boundaries(const std::span<const glm::uvec3> triangles) {
    DEBUG_ASSERT(is_manifold(triangles));
    std::unordered_set<glm::uvec2> boundary_edges = find_boundary_edges(triangles);
    std::unordered_map<uint32_t, std::vector<uint32_t>> adjacencies;
    adjacencies.reserve((boundary_edges.size() * 3) / 2);
    for (const glm::uvec2 &edge : boundary_edges) {
        adjacencies[edge[0]].push_back(edge[1]);
    }

    auto remove_edge = [&](const auto edge) {
        boundary_edges.erase(edge);
        remove_first(adjacencies[edge[0]], edge[1]);
    };

    std::vector<std::vector<uint32_t>> boundaries;
    while (!boundary_edges.empty()) {
        std::vector<uint32_t> boundary;
        
        const glm::uvec2 starting_edge = *boundary_edges.begin();
        remove_edge(starting_edge);

        const uint32_t starting_vertex_id = starting_edge[0];

        uint32_t current_vertex_id = starting_edge[1];
        boundary.push_back(current_vertex_id);
        while (true) {
            const auto& neighbours = adjacencies[current_vertex_id];
            DEBUG_ASSERT(neighbours.size() >= 1);
            
            const uint32_t next_vertex_id = neighbours[0];
            const glm::uvec2 edge(current_vertex_id, next_vertex_id);
            remove_edge(edge);

            boundary.push_back(next_vertex_id);
            if (next_vertex_id == starting_vertex_id) {
                break;
            }
            current_vertex_id = next_vertex_id;
        }

        boundaries.push_back(std::move(boundary));
    }

    return boundaries;
}

std::vector<std::vector<uint32_t>> find_boundaries_non_manifold(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<glm::uvec2> boundary_edges = find_boundary_edges(triangles);
    std::unordered_map<uint32_t, std::vector<uint32_t>> adjacencies;
    adjacencies.reserve((boundary_edges.size() * 3) / 2);
    for (const glm::uvec2 &edge : boundary_edges) {
        adjacencies[edge[0]].push_back(edge[1]);
    }

    auto remove_edge = [&](const auto edge) {
        boundary_edges.erase(edge);
        remove_first(adjacencies[edge[0]], edge[1]);
    };

    std::vector<std::vector<uint32_t>> boundaries;
    while (!boundary_edges.empty()) {
        const glm::uvec2 starting_edge = *boundary_edges.begin();
        remove_edge(starting_edge);

        std::vector<uint32_t> boundary;
        const uint32_t starting_vertex_id = starting_edge[0];

        uint32_t current_vertex_id = starting_edge[1];
        boundary.push_back(current_vertex_id);
        while (true) {
            const auto &neighbours = adjacencies[current_vertex_id];
            if (neighbours.empty()) {
                break;
            }
            const uint32_t next_vertex_id = neighbours[0];
            const glm::uvec2 edge(current_vertex_id, next_vertex_id);
            remove_edge(edge);

            boundary.push_back(next_vertex_id);
            if (next_vertex_id == starting_vertex_id) {
                break;
            }
            current_vertex_id = next_vertex_id;
        }

        if (boundary.empty()) {
            continue;
        }

        // split boundary into individual loops
        std::unordered_map<uint32_t, uint32_t> visited;
        for (uint32_t i = 0; i < boundary.size(); i++) {
            const uint32_t vertex = boundary[i];
            auto it = visited.find(vertex);
            if (it != visited.end()) {
                const uint32_t first_occurance = it->second;
                // finished current loop
                DEBUG_ASSERT(first_occurance < i);
                auto loop_start = boundary.begin() + first_occurance;
                auto loop_end = boundary.begin() + i;
                std::vector<uint32_t> loop(loop_start, loop_end);
                boundaries.push_back(std::move(loop));
                boundary.erase(loop_start, loop_end);
                i = first_occurance;
            } else {
                visited.emplace(vertex, i);
            }
        }

        if (boundary.empty()) {
            continue;
        }

        boundaries.push_back(std::move(boundary));
    }

    return boundaries;
}
}
