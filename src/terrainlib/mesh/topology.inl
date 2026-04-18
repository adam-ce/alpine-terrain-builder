#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "FixedVector.h"
#include "HybridVector.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

inline constexpr bool is_degenerate(const glm::uvec3 &triangle) {
    return triangle[0] == triangle[1] ||
           triangle[1] == triangle[2] ||
           triangle[2] == triangle[0];
}

template <glm::length_t n_dims, typename T>
constexpr void flip_orientation(mesh::Simple_<n_dims, T> &mesh) {
    flip_triangle_orientations(mesh.triangles);
}
constexpr void flip_triangle_orientations(std::span<glm::uvec3> triangles) {
    for (auto &triangle : triangles) {
        flip_triangle_orientation(triangle);
    }
}

template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const mesh::View_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_non_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const mesh::View_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_non_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const mesh::Simple_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_non_manifold2(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const mesh::View_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_non_manifold2(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const mesh::Simple_<n_dims, T> &mesh) {
    return create_vertex_to_triangle_mapping(mesh.triangles, mesh.vertex_count() - 1);
}
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const mesh::View_<n_dims, T> &mesh) {
    return create_vertex_to_triangle_mapping(mesh.triangles, mesh.vertex_count() - 1);
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::Simple_<n_dims, T> &mesh) {
    return count_vertex_adjacent_triangles(mesh.triangles, mesh.vertex_count() - 1);
}
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::View_<n_dims, T> &mesh) {
    return count_vertex_adjacent_triangles(mesh.triangles, mesh.vertex_count() - 1);
}

inline constexpr glm::uvec2 other_vertices_in_triangle(const glm::uvec3 &triangle, const uint32_t vertex) {
    if (triangle.x == vertex) {
        return glm::uvec2(triangle.y, triangle.z);
    }
    if (triangle.y == vertex) {
        return glm::uvec2(triangle.x, triangle.z);
    }
    DEBUG_ASSERT(triangle.z == vertex);
    return glm::uvec2(triangle.x, triangle.y);
}

inline constexpr void change_vertex_inplace(glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex, const bool allow_missing) {
    DEBUG_ASSERT(!is_degenerate(triangle));
    if (triangle.x == old_vertex) {
        triangle.x = new_vertex;
    } else if (triangle.y == old_vertex) {
        triangle.y = new_vertex;
    } else if (triangle.z == old_vertex) {
        triangle.z = new_vertex;
    } else if (!allow_missing) {
        UNREACHABLE();
    }
}

inline constexpr glm::uvec3 change_vertex(const glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex, const bool allow_missing) {
    DEBUG_ASSERT(!is_degenerate(triangle));
    if (triangle.x == old_vertex) {
        return glm::uvec3(new_vertex, triangle.y, triangle.z);
    } else if (triangle.y == old_vertex) {
        return glm::uvec3(triangle.x, new_vertex, triangle.z);
    } else if (triangle.z == old_vertex) {
        return glm::uvec3(triangle.x, triangle.y, new_vertex);
    } else if (!allow_missing) {
        UNREACHABLE();
    }
}

inline constexpr void flip_triangle_orientation(glm::uvec3 &triangle) {
    std::swap(triangle.z, triangle.y);
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const mesh::Simple_<n_dims, T> &mesh) {
    return find_isolated_vertices(mesh.triangles, mesh.vertex_count() - 1);
}
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const mesh::View_<n_dims, T> &mesh) {
    return find_isolated_vertices(mesh.triangles, mesh.vertex_count() - 1);
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const mesh::Simple_<n_dims, T> &mesh) {
    return build_vertex_adjacency(mesh.triangles, mesh.vertex_count() - 1);
}
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> build_vertex_adjacency(const mesh::View_<n_dims, T> &mesh) {
    return build_vertex_adjacency(mesh.triangles, mesh.vertex_count() - 1);
}

template <glm::length_t n_dims, typename T>
bool is_orientable(const mesh::Simple_<n_dims, T> &mesh) {
    return is_orientable(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
bool is_orientable(const mesh::View_<n_dims, T> &mesh) {
    return is_orientable(mesh.triangles);
}

}
