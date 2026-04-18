#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "UnionFind.h"
#include "mesh/SimpleMesh.h"
#include "mesh/cleanup.h"
#include "mesh/topology.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> find_non_manifold_edges(const mesh::Simple_<n_dims, T> &mesh) {
    return find_non_manifold_edges(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> find_non_manifold_edges(const mesh::View_<n_dims, T> &mesh) {
    return find_non_manifold_edges(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_edges(mesh::Simple_<n_dims, T> &mesh) {
    duplicate_non_manifold_edges(mesh.triangles, mesh.positions, mesh.uvs);
}
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions) {
    std::vector<glm::vec<2, double>> uvs; // empty uvs
    duplicate_non_manifold_edges(triangles, positions, uvs);
}
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs) {
    auto duplicate_vertex = [&](const uint32_t old_vertex_index) {
        const uint32_t new_vertex_index = positions.size();
        positions.push_back(positions[old_vertex_index]);
        if (uvs.size() > 0) {
            uvs.push_back(uvs[old_vertex_index]);
        }
        return new_vertex_index;
    };
    duplicate_non_manifold_edges(triangles, duplicate_vertex);

#ifndef NDEBUG
    const std::vector<glm::uvec3> triangles_copy(triangles.begin(), triangles.end());
    const mesh::Simple_<n_dims, Position> mesh(triangles_copy, positions);
    DEBUG_ASSERT(find_non_manifold_edges(mesh).empty());
#endif
}
template <typename Duplicate>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    Duplicate &&duplicate_vertex) {
    std::unordered_map<glm::uvec2, uint32_t> triangles_per_edge;

    for (glm::uvec3 &triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            const uint8_t next_k = (k + 1) % 3;
            const glm::vec<2, uint8_t> edge_indices(k, next_k);
            const glm::uvec2 edge(triangle[edge_indices[0]], triangle[edge_indices[1]]);
            uint32_t &triangle_count = triangles_per_edge[normalize_edge(edge)];
            if (triangle_count < 2) {
                triangle_count++;
            } else {
                // edge already has two triangles -> duplicate first edge vertex
                const uint32_t vertex_index = edge[0];
                const uint32_t duplicate_vertex_index = duplicate_vertex(vertex_index);
                glm::uvec2 new_edge(duplicate_vertex_index, edge[1]);
                triangle[edge_indices[0]] = duplicate_vertex_index;
                triangles_per_edge[normalize_edge(new_edge)]++;
            }
        }
    }

    DEBUG_ASSERT(is_edge_manifold(triangles));
}

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_vertices(mesh::Simple_<n_dims, T> &mesh) {
    duplicate_non_manifold_vertices(mesh.triangles, mesh.positions, mesh.uvs);
}
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions) {
    std::vector<glm::vec<2, double>> uvs; // empty uvs
    duplicate_non_manifold_vertices(triangles, positions, uvs);
}
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs) {
    auto duplicate_vertex = [&](const uint32_t old_vertex_index) {
        const uint32_t new_vertex_index = positions.size();
        positions.push_back(positions[old_vertex_index]);
        if (uvs.size() > 0) {
            uvs.push_back(uvs[old_vertex_index]);
        }
        return new_vertex_index;
    };
    duplicate_non_manifold_vertices(triangles, positions.size(), duplicate_vertex);
}
template <typename Duplicate>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    uint32_t vertex_count,
    Duplicate &&duplicate_vertex) {
    bool changed;
    do {
        DEBUG_ASSERT(vertex_count >= compute_vertex_count(triangles));
        changed = false;
        const std::vector<std::vector<uint32_t>> vertex_to_triangles = create_vertex_to_triangle_mapping(triangles, vertex_count);

        // neighbor vertex -> local triangle indices sharing edge (vertex, neighbor)
        std::unordered_map<uint32_t, std::vector<uint32_t>> edge_groups;
        UnionFind_<true, uint32_t, uint32_t> union_find;
        for (uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
            const auto &incident_triangles = vertex_to_triangles[vertex_index];
            const uint32_t incident_count = incident_triangles.size();

            if (incident_count <= 1) {
                continue;
            }

            // Group neighbouring triangles by shared edges
            edge_groups.clear();
            edge_groups.reserve(incident_count * 2);

            for (uint32_t local_index = 0; local_index < incident_count; local_index++) {
                const glm::uvec3 &triangle = triangles[incident_triangles[local_index]];
                const glm::uvec2 other_vertices = other_vertices_in_triangle(triangle, vertex_index);
                edge_groups[other_vertices.x].push_back(local_index);
                edge_groups[other_vertices.y].push_back(local_index);
            }

            // Union triangles that share edge (vertex, neighbor)
            union_find.reset(incident_count);
            for (const auto &[_, edge_group] : edge_groups) {
                for (uint32_t i = 1; i < edge_group.size(); i++) {
                    union_find.make_union(edge_group[0], edge_group[i]);
                }
            }

            if (union_find.is_joint()) {
                continue;
            }

            // Collect connected face fans
            std::unordered_map<uint32_t, std::vector<uint32_t>> face_fans = union_find.get_sets();
            const auto largest_fan_it = std::max_element(
                face_fans.begin(),
                face_fans.end(),
                [](const auto &a, const auto &b) {
                    return a.second.size() < b.second.size();
                });
            face_fans.erase(largest_fan_it);
            DEBUG_ASSERT(!face_fans.empty());

            // Duplicate once per remaining fan
            for (const auto &[_, face_fan] : face_fans) {
                const uint32_t duplicated_index = duplicate_vertex(vertex_index);
                vertex_count++;

                for (const uint32_t local_triangle_index : face_fan) {
                    glm::uvec3 &triangle = triangles[incident_triangles[local_triangle_index]];
                    change_vertex_inplace(triangle, vertex_index, duplicated_index);
                }
            }
            
            changed = true;
            break;
        }
    } while (changed);

    DEBUG_ASSERT(is_vertex_manifold(triangles));
}

template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> &mesh) {
    make_manifold(mesh.triangles, mesh.positions, mesh.uvs);
}
template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions) {
    std::vector<glm::vec<2, double>> uvs; // empty uvs
    make_manifold(triangles, positions, uvs);
}
template <glm::length_t n_dims, typename Position, typename Uv>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs) {
    remove_degenerate_triangles(triangles);
    duplicate_non_manifold_edges(triangles, positions, uvs);
    duplicate_non_manifold_vertices(triangles, positions, uvs);
    DEBUG_ASSERT(is_manifold(triangles));
}

template <typename Duplicate>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    uint32_t vertex_count,
    Duplicate &&duplicate_vertex) {
    // We need a wrapped overload to ensure vertex_count stays correct.
    auto new_duplicate_vertex = [&](const uint32_t vertex_index) -> uint32_t {
        vertex_count++;
        const uint32_t duplicated_index = duplicate_vertex(vertex_index);
        DEBUG_ASSERT(duplicated_index != vertex_index);
        return duplicated_index;
    };

    remove_degenerate_triangles(triangles);

    // do not reorder
    duplicate_non_manifold_edges(triangles, new_duplicate_vertex);
    duplicate_non_manifold_vertices(triangles, vertex_count, new_duplicate_vertex);

    DEBUG_ASSERT(vertex_count >= compute_vertex_count(triangles));
    DEBUG_ASSERT(is_manifold(triangles));
}

template <glm::length_t n_dims, typename T>
bool is_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return is_manifold(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
bool is_manifold(const mesh::View_<n_dims, T> &mesh) {
    return is_manifold(mesh.triangles, mesh.vertex_count());
}

template <glm::length_t n_dims, typename T>
bool is_edge_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return is_edge_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
bool is_edge_manifold(const mesh::View_<n_dims, T> &mesh) {
    return is_edge_manifold(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
bool is_vertex_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return is_vertex_manifold(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
bool is_vertex_manifold(const mesh::View_<n_dims, T> &mesh) {
    return is_vertex_manifold(mesh.triangles, mesh.vertex_count());
}
} // namespace mesh
