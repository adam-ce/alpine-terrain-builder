#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

namespace geometry {

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
    const glm::vec<n_dims, T> e1 = v1 - v0;
    const glm::vec<n_dims, T> e2 = v2 - v0;

    const T e1e1 = glm::dot(e1, e1);
    const T e2e2 = glm::dot(e2, e2);
    const T e1e2 = glm::dot(e1, e2);

    // squared parallelogram area = |e1|^2 |e2|^2 - (e1 . e2)^2
    // squared triangle area = squared parallelogram area / 4
    const T parallelogram_area2 = std::fma(e1e1, e2e2, -(e1e2 * e1e2));

    // Small negative values can occur from roundoff for nearly degenerate triangles.
    return std::max(T(0), parallelogram_area2) / T(4);
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

template <typename T>
T compute_signed_triangle_area(const glm::vec<2, T> &v0, const glm::vec<2, T> &v1, const glm::vec<2, T> &v2) {
    return orient(v0, v1, v2) / T(2);
}
template <typename T>
T compute_signed_triangle_area(const std::array<glm::vec<2, T>, 3> &triangle) {
    return compute_signed_triangle_area<T>(triangle[0], triangle[1], triangle[2]);
}
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<2, T>> positions) {
    return compute_signed_triangle_area<T>(positions[triangle.x], positions[triangle.y], positions[triangle.z]);
}
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<2, T>> &positions) {
    return compute_signed_triangle_area<T>(triangle, std::span<const glm::vec<2, T>>(positions));
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

template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2, const T min_area) {
    return compute_squared_triangle_area(v0, v1, v2) <= min_area * min_area;
}
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const std::array<glm::vec<n_dims, T>, 3> &triangle, const T min_area) {
    return compute_squared_triangle_area(triangle) <= min_area * min_area;
}
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions, const T min_area) {
    return compute_squared_triangle_area(triangle, positions) <= min_area * min_area;
}
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions, const T min_area) {
    return compute_squared_triangle_area(triangle, positions) <= min_area * min_area;
}

} // namespace geometry