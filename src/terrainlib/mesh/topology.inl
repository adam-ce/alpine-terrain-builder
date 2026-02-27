#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "FixedVector.h"
#include "HybridVector.h"
#include "mesh/SimpleMesh.h"
#include "mesh/TriangleContainer.h"

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(mesh::Simple_<n_dims, T> &mesh) {
    sort_and_normalize_triangles(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
void flip_orientation(mesh::Simple_<n_dims, T> &mesh) {
    flip_triangle_orientations(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::unordered_set<typename mesh::Simple_<n_dims, T>::Edge> get_edges(const mesh::Simple_<n_dims, T> &mesh, const bool normalize) {
    using Edge = typename mesh::Simple_<n_dims, T>::Edge;
    std::unordered_set<Edge> edges;
    for_each_edge(mesh, [&](const Edge &edge, const uint32_t /*triangle_index*/) { 
        edges.insert(edge); 
    }, normalize);
    return edges;
}

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F &&func, const bool normalize) {
    for_each_edge(mesh.triangles, std::forward<F>(func), normalize);
}
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func, const bool normalize) {
    for (uint32_t i = 0; i < triangles.size(); i++) {
        glm::uvec3 triangle = triangles[i];
        if (normalize) {
            normalize_triangle_inplace(triangle, false);
        }

        func(glm::uvec2(triangle[0], triangle[1]), i);
        func(glm::uvec2(triangle[1], triangle[2]), i);
        if (normalize) {
            func(glm::uvec2(triangle[0], triangle[2]), i);
        } else {
            func(glm::uvec2(triangle[2], triangle[0]), i);
        }
    }
}

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_edges(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_edges(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_triangles(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_triangles(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, FixedVector<uint32_t, 2>> create_edge_to_triangle_mapping_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> create_edge_to_triangle_mapping_non_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_non_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_map<glm::uvec2, std::vector<uint32_t>> create_edge_to_triangle_mapping_non_manifold2(const mesh::Simple_<n_dims, T> &mesh) {
    return create_edge_to_triangle_mapping_non_manifold2(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const mesh::Simple_<n_dims, T> &mesh) {
    return create_vertex_to_triangle_mapping(mesh.triangles, mesh.vertex_count());
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> count_vertex_adjacent_triangles(const mesh::Simple_<n_dims, T> &mesh) {
    return count_vertex_adjacent_triangles(mesh.triangles, mesh.vertex_count());
}

// micro utils we want to be inlined
namespace detail {
template <size_t N>
inline void normalize_face_index_rotation_impl(std::span<uint32_t, N> face, bool keep_orientation) {
    if (keep_orientation) {
        if (face.empty()) {
            return;
        }

        // find index of minimum element
        size_t min_index = 0;
        for (size_t k = 1; k < face.size(); k++) {
            if (face[k] < face[min_index]) {
                min_index = k;
            }
        }

        // rotate so minimum is first
        if (min_index != 0) {
            std::rotate(face.begin(), face.begin() + min_index, face.end());
        }
    } else {
        std::sort(face.begin(), face.end());
    }
}
} // namespace detail

inline constexpr glm::uvec2 other_vertices_in_triangle(const glm::uvec3 &triangle, const uint32_t vertex) {
    if (triangle.x == vertex) {
        return glm::uvec2(triangle.y, triangle.z);
    }
    if (triangle.y == vertex) {
        return glm::uvec2(triangle.x, triangle.z);
    }
    return glm::uvec2(triangle.x, triangle.y);
}

inline constexpr void change_vertex_inplace(glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex) {
    if (triangle.x == old_vertex) {
        triangle.x = new_vertex;
        return;
    }
    if (triangle.y == old_vertex) {
        triangle.y = new_vertex;
        return;
    }
    if (triangle.z == old_vertex) {
        triangle.z = new_vertex;
        return;
    }
}

inline constexpr glm::uvec3 change_vertex(const glm::uvec3 &triangle, const uint32_t old_vertex, const uint32_t new_vertex) {
    if (triangle.x == old_vertex) {
        return glm::uvec3(new_vertex, triangle.y, triangle.z);
    } else if (triangle.y == old_vertex) {
        return glm::uvec3(triangle.x, new_vertex, triangle.z);
    } else if (triangle.z == old_vertex) {
        return glm::uvec3(triangle.x, triangle.y, new_vertex);
    } else {
        return triangle;
    }
}

inline void normalize_face_index_rotation(const std::span<uint32_t> face, bool keep_orientation) {
    detail::normalize_face_index_rotation_impl<std::dynamic_extent>(face, keep_orientation);
}

inline constexpr void normalize_edge_inplace(glm::uvec2 &edge) {
    if (edge.x > edge.y) {
        std::swap(edge.x, edge.y);
    }
}
inline constexpr glm::uvec2 normalize_edge(glm::uvec2 edge) {
    normalize_edge_inplace(edge);
    return edge;
}

inline void normalize_triangle_inplace(glm::uvec3 &triangle, const bool keep_orientation) {
    std::span<uint32_t, 3> data(glm::value_ptr(triangle), triangle.length());
    detail::normalize_face_index_rotation_impl<3>(data, keep_orientation);
}
inline glm::uvec3 normalize_triangle(glm::uvec3 triangle, const bool keep_orientation) {
    normalize_triangle_inplace(triangle, keep_orientation);
    return triangle;
}

inline void normalize_quad_inplace(glm::uvec4 &quad, const bool keep_orientation) {
    std::span<uint32_t, 4> data(glm::value_ptr(quad), quad.length());
    detail::normalize_face_index_rotation_impl<4>(data, keep_orientation);
}
inline glm::uvec4 normalize_quad(glm::uvec4 quad, const bool keep_orientation) {
    normalize_quad_inplace(quad, keep_orientation);
    return quad;
}

inline constexpr bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    // First, compare by x
    if (t1.x != t2.x) {
        return t1.x < t2.x;
    }

    // If x is equal, compare by y
    if (t1.y != t2.y) {
        return t1.y < t2.y;
    }

    // If x and y are equal, compare by z
    return t1.z < t2.z;
}

inline bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    glm::uvec3 t1s(t1);
    glm::uvec3 t2s(t2);

    std::sort(&t1s.x, &t1s.z + 1);
    std::sort(&t2s.x, &t2s.z + 1);

    return compare_triangles(t1s, t2s);
}

inline bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return normalize_triangle(t1) == normalize_triangle(t2);
}
inline bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1,
                                                                    const glm::uvec3 &t2) {
    return std::is_permutation(&t1.x, &t1.z + 1, &t2.x);
}

inline constexpr void flip_triangle_orientation(glm::uvec3 &triangle) {
    std::swap(triangle.z, triangle.x);
}

inline std::vector<std::vector<uint32_t>> create_vertex_to_triangle_mapping(const SimpleMesh &mesh) {
    return create_vertex_to_triangle_mapping(mesh.triangles, mesh.vertex_count());
}

inline std::vector<uint32_t> count_vertex_adjacent_triangles(const SimpleMesh &mesh) {
    return count_vertex_adjacent_triangles(mesh.triangles, mesh.vertex_count());
}

inline std::unordered_set<glm::uvec2> find_boundary_edges(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<glm::uvec2> boundary;
    find_boundary_edges(triangles, boundary);
    return boundary;
}
