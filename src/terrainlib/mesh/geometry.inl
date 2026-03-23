#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                              const glm::vec<3, T> &b,
                              const glm::vec<3, T> &c,
                              const bool normalize) {
    const glm::vec<3, T> n = glm::cross(b - a, c - a);
    if (normalize && glm::length2(n) > T(0)) {
        return glm::normalize(n);
    }
    return n;
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                              const std::span<const glm::vec<3, T>> positions,
                              const bool normalize) {
    const auto &a = positions[triangle[0]];
    const auto &b = positions[triangle[1]];
    const auto &c = positions[triangle[2]];
    return compute_normal(a, b, c, normalize);
}


template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2) {
    if constexpr (n_dims == 3) {
        const glm::vec<3, T> edge1 = v1 - v0;
        const glm::vec<3, T> edge2 = v2 - v0;
        return glm::length2(glm::cross(edge1, edge2)) / 4;
    } else {
        const T a = glm::distance(v0, v1);
        const T b = glm::distance(v1, v2);
        const T c = glm::distance(v2, v0);
        const T s = (a + b + c) / 2;
        return s * (s - a) * (s - b) * (s - c);
    }
}
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle) {
    return compute_squared_triangle_area<n_dims, T>(triangle[0], triangle[1], triangle[2]);
}
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions) {
    const glm::vec<n_dims, T> &v0 = positions[triangle.x];
    const glm::vec<n_dims, T> &v1 = positions[triangle.y];
    const glm::vec<n_dims, T> &v2 = positions[triangle.z];
    return compute_squared_triangle_area<n_dims, T>(v0, v1, v2);
}
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>>& positions) {
    return compute_squared_triangle_area<n_dims, T>(triangle, std::span(positions));
}

template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2) {
    return std::sqrt(compute_squared_triangle_area(v0, v1, v2));
}
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle) {
    return std::sqrt(compute_squared_triangle_area(triangle));
}
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions) {
    return std::sqrt(compute_squared_triangle_area(triangle, positions));
}
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>>& positions) {
    return std::sqrt(compute_squared_triangle_area(triangle, positions));
}

namespace mesh {

template <glm::length_t n_dims, typename T>
T compute_surface_area(const std::span<const glm::uvec3> triangles, const std::span<const glm::vec<n_dims, T>> positions) {
    T total_area = static_cast<T>(0);
    for (const glm::uvec3& triangle : triangles) {
        const T triangle_area = compute_triangle_area<n_dims, T>(triangle, positions);
        total_area += triangle_area;
    }
    return total_area;
}
template <glm::length_t n_dims, typename T>
T compute_surface_area(const std::vector<glm::uvec3>& triangles, const std::vector<glm::vec<n_dims, T>>& positions) {
    return compute_surface_area<n_dims, T>(std::span<const glm::uvec3>(triangles), std::span<const glm::vec<n_dims, T>>(positions));
}
template <glm::length_t n_dims, typename T>
T compute_surface_area(const mesh::View_<n_dims, T>& mesh) {
    return compute_surface_area<n_dims, T>(mesh.triangles, mesh.positions);
}
template <glm::length_t n_dims, typename T>
T compute_surface_area(const mesh::Simple_<n_dims, T>& mesh) {
    return compute_surface_area<n_dims, T>(mesh.triangles, mesh.positions);
}

} // namespace mesh
