#include <span>
#include <unordered_set>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/edges.h"

namespace mesh {
    
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_edges(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_edges(mesh.triangles);
}
inline std::unordered_set<glm::uvec2> find_boundary_edges(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<glm::uvec2> boundary;
    find_boundary_edges(triangles, boundary);
    return boundary;
}

template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_triangles(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_triangles(mesh.triangles);
}
inline std::unordered_set<uint32_t> find_boundary_triangles(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<uint32_t> boundary;
    find_boundary_triangles(triangles, boundary);
    return boundary;
}

template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_vertices(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_vertices(mesh.triangles);
}
inline std::unordered_set<uint32_t> find_boundary_vertices(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<uint32_t> boundary;
    find_boundary_vertices(triangles, boundary);
    return boundary;
}

template <glm::length_t n_dims, typename T>
inline std::vector<uint8_t> build_boundary_vertex_mask(const mesh::Simple_<n_dims, T> &mesh) {
    return build_boundary_vertex_mask(mesh.triangles, mesh.vertex_count());
}
inline std::vector<uint8_t> build_boundary_vertex_mask(const std::span<const glm::uvec3> triangles, size_t vertex_count) {
    std::vector<uint8_t> mask;
    build_boundary_vertex_mask<uint8_t>(triangles, vertex_count, mask, 0, 1);
    return mask;
}

template <typename MaskT>
inline void build_boundary_vertex_mask(const std::span<const glm::uvec3> triangles,
                                      const size_t vertex_count,
                                      std::vector<MaskT> &boundary,
                                      const MaskT on_boundary,
                                      const MaskT not_on_boundary) {
    std::unordered_set<glm::uvec2> boundary_edges = find_boundary_edges(triangles);

    boundary.assign(vertex_count, not_on_boundary);
    for (const glm::uvec2 edge : boundary_edges) {
        boundary[edge.x] = on_boundary;
        boundary[edge.y] = on_boundary;
    }
}

namespace detail {
inline std::unordered_map<glm::uvec2, uint32_t> find_boundary_edges_and_triangles(const std::span<const glm::uvec3> triangles) {
    std::unordered_map<glm::uvec2, uint32_t> edge_to_triangle;
    for_each_halfedge(triangles, [&](const glm::uvec2 &edge, const uint32_t triangle_index) {
        auto it = edge_to_triangle.find(glm::uvec2(edge.y, edge.x));
        if (it != edge_to_triangle.end()) {
            edge_to_triangle.erase(it);
        } else {
            edge_to_triangle[edge] = triangle_index;
        }
    }, /* normalize */ false);
    return edge_to_triangle;
}
}

template <typename MaskT>
inline void build_boundary_triangle_mask(const std::span<const glm::uvec3> triangles,
                                        std::vector<MaskT> &boundary,
                                        const MaskT on_boundary,
                                        const MaskT not_on_boundary) {
    const std::unordered_map<glm::uvec2, uint32_t> edge_to_triangle = detail::find_boundary_edges_and_triangles(triangles);
    boundary.assign(triangles.size(), not_on_boundary);
    for (const auto &[_, triangle_index] : edge_to_triangle) {
        boundary[triangle_index] = on_boundary;
    }
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundaries(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries_non_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundaries_non_manifold(mesh.triangles);
}

}
