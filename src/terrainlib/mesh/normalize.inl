#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/triangle_compare.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(mesh::Simple_<n_dims, T> &mesh) {
    sort_and_normalize_triangles(mesh.triangles);
}
inline void sort_and_normalize_triangles(std::span<glm::uvec3> triangles) {
    normalize_triangles_inplace(triangles);
    sort_triangles(triangles);
}
inline void sort_triangles(std::span<glm::uvec3> triangles) {
    std::sort(triangles.begin(), triangles.end(), compare_triangles);
}
template <glm::length_t n_dims, typename T>
void sort_triangles(mesh::Simple_<n_dims, T> &mesh) {
    sort_triangles(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
void normalize_triangles(mesh::Simple_<n_dims, T> &mesh) {
    normalize_triangles(mesh.triangles);
}

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

inline void normalize_triangles_inplace(std::span<glm::uvec3> triangles, const bool keep_orientation) {
    for (glm::uvec3 &triangle : triangles) {
        normalize_triangle_inplace(triangle, keep_orientation);
    }
}
inline void normalize_triangles_inplace(std::vector<glm::uvec3> &triangles, const bool keep_orientation) {
    normalize_triangles_inplace(std::span(triangles), keep_orientation);
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

}
