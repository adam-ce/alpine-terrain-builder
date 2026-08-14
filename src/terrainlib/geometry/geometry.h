#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

namespace geometry {

// Grow bounds by the same distance on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_absolute(const radix::geometry::Aabb<n_dims, T> &bounds, const T padding) {
    using Vec = glm::vec<n_dims, T>;
    return radix::geometry::Aabb<n_dims, T>(bounds.min - Vec(padding), bounds.max + Vec(padding));
}

// Grow bounds by the given fraction of their size on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_relative(const radix::geometry::Aabb<n_dims, T> &bounds, const T fraction, const T min_padding = T(0)) {
    using Vec = glm::vec<n_dims, T>;
    const Vec padding = glm::max(bounds.size() * fraction, Vec(min_padding));
    return radix::geometry::Aabb<n_dims, T>(bounds.min - padding, bounds.max + padding);
}

// Normalize a set of positions into the range of [-1,1] based on maximum extents of the bounding box.
// Outputs are written as float coordinates.
// Optionally outputs the computed AABB if out_bounds is provided.
template <glm::length_t n_dims>
void to_approximate_normalized(
    std::span<const glm::vec<n_dims, double>> positions,
    std::vector<glm::vec<n_dims, float>> &approx,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr) {
    // compute bounds
    const radix::geometry::Aabb<n_dims, double> bounds = radix::geometry::find_bounds(positions);
    const glm::vec<n_dims, double> center = bounds.centre();
    const glm::vec<n_dims, double> extents = bounds.size() / 2.0;
    const double max_extents = glm::compMax(extents);

    if (out_bounds) {
        *out_bounds = bounds;
    }

    // normalize based on aabb
    approx.clear();
    approx.reserve(positions.size());
    for (const auto &p : positions) {
        const glm::vec<n_dims, double> rel = (p - center) / max_extents;
        approx.push_back(glm::vec<n_dims, float>(rel));
    }
}
template <glm::length_t n_dims>
void to_approximate_normalized(
    const std::vector<glm::vec<n_dims, double>> &positions,
    std::vector<glm::vec<n_dims, float>> &approx,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr) {
    return to_approximate_normalized(std::span(positions), approx, out_bounds);
}

// Normalize a set of positions into the range of [-1,1] based on maximum extents of the bounding box.
// Outputs are written as float coordinates.
// Optionally outputs the computed AABB if out_bounds is provided.
template <glm::length_t n_dims>
std::vector<glm::vec<n_dims, float>> to_approximate_normalized(
    std::span<const glm::vec<n_dims, double>> positions,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr) {
    std::vector<glm::vec<n_dims, float>> approx;
    to_approximate_normalized(positions, approx, out_bounds);
    return approx;
}
template <glm::length_t n_dims>
std::vector<glm::vec<n_dims, float>> to_approximate_normalized(
    const std::vector<glm::vec<n_dims, double>> &positions,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr) {
    return to_approximate_normalized(std::span(positions), out_bounds);
}

template <glm::length_t n_dims, typename T>
radix::geometry::Triangle<n_dims, T> corners(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions) {
    return {positions[triangle.x], positions[triangle.y], positions[triangle.z]};
}

// Scalar z-component in 2D, full cross product vector in 3D.
template <glm::length_t n_dims, typename T>
auto cross(const glm::vec<n_dims, T> &a, const glm::vec<n_dims, T> &b) {
    if constexpr (n_dims == 2) {
        return a.x * b.y - a.y * b.x;
    } else if constexpr (n_dims == 3) {
        return glm::cross(a, b);
    } else {
        static_assert(n_dims == 2 || n_dims == 3, "cross is only defined for 2D and 3D vectors");
    }
}

// Positive when p lies left of the directed edge a to b, and zero when the three are collinear.
template <typename T>
T orient(const glm::vec<2, T> &a, const glm::vec<2, T> &b, const glm::vec<2, T> &p) {
    return cross(b - a, p - a);
}

// Corner weights of the point, summing to one. Meaningless for a degenerate triangle.
template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_barycentric(
    const glm::vec<n_dims, T> &point,
    const glm::vec<n_dims, T> &a,
    const glm::vec<n_dims, T> &b,
    const glm::vec<n_dims, T> &c) {
    if constexpr (n_dims == 2) {
        const T denom = orient(a, b, c);
        const T first = orient(b, c, point) / denom;
        const T second = orient(c, a, point) / denom;
        return {first, second, T(1) - first - second};
    } else {
        using Vec = glm::vec<n_dims, T>;

        const Vec v0 = b - a;
        const Vec v1 = c - a;
        const Vec v2 = point - a;

        const T d00 = glm::dot(v0, v0);
        const T d01 = glm::dot(v0, v1);
        const T d11 = glm::dot(v1, v1);
        const T d20 = glm::dot(v2, v0);
        const T d21 = glm::dot(v2, v1);

        const T denom = d00 * d11 - d01 * d01;
        ASSERT(denom != 0);
        const T inv_denom = 1 / denom;

        const T v = (d11 * d20 - d01 * d21) * inv_denom;
        const T w = (d00 * d21 - d01 * d20) * inv_denom;
        const T u = 1 - v - w;

        return Vec(u, v, w);
    }
}

template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_barycentric(const glm::vec<n_dims, T> &point, const radix::geometry::Triangle<n_dims, T> &triangle) {
    return compute_barycentric(point, triangle[0], triangle[1], triangle[2]);
}

template <typename V, typename T>
V interpolate(const std::array<V, 3> &values, const glm::vec<3, T> &weights) {
    return weights[0] * values[0] + weights[1] * values[1] + weights[2] * values[2];
}

template <glm::length_t n_dims, typename T>
T distance_to_segment(const glm::vec<n_dims, T> &point, const radix::geometry::Edge<n_dims, T> &segment) {
    constexpr T epsilon_squared = radix::geometry::epsilon<T> * radix::geometry::epsilon<T>;

    const glm::vec<n_dims, T> along = segment[1] - segment[0];
    const T length_squared = glm::dot(along, along);
    if (length_squared <= epsilon_squared) {
        return glm::distance(point, segment[0]);
    }
    const T t = std::clamp(glm::dot(point - segment[0], along) / length_squared, T(0), T(1));
    return glm::distance(point, segment[0] + t * along);
}

// Zero inside the triangle, growing outside it. Accepts either winding.
template <typename T>
T distance_to_triangle(const glm::vec<2, T> &point, const radix::geometry::Triangle<2, T> &triangle) {
    const T orientation = cross(triangle[1] - triangle[0], triangle[2] - triangle[0]);

    bool inside = orientation != T(0);
    for (uint8_t corner = 0; inside && corner < 3; corner++) {
        const glm::vec<2, T> edge = triangle[(corner + 1) % 3] - triangle[corner];
        inside = cross(edge, point - triangle[corner]) * orientation >= T(0);
    }
    if (inside) {
        return T(0);
    }

    T nearest = std::numeric_limits<T>::max();
    for (uint8_t corner = 0; corner < 3; corner++) {
        const radix::geometry::Edge<2, T> edge = {triangle[corner], triangle[(corner + 1) % 3]};
        nearest = std::min(nearest, distance_to_segment(point, edge));
    }
    return nearest;
}

namespace detail {
// True when an edge normal of the first triangle separates the two. Touching does not separate.
template <typename T>
bool separates(const radix::geometry::Triangle<2, T> &triangle, const radix::geometry::Triangle<2, T> &other) {
    constexpr T epsilon_squared = radix::geometry::epsilon<T> * radix::geometry::epsilon<T>;

    const auto extent_along = [](const glm::vec<2, T> &axis, const radix::geometry::Triangle<2, T> &corners) {
        T lowest = std::numeric_limits<T>::max();
        T highest = std::numeric_limits<T>::lowest();
        for (const glm::vec<2, T> &corner : corners) {
            const T value = glm::dot(axis, corner);
            lowest = std::min(lowest, value);
            highest = std::max(highest, value);
        }
        return glm::vec<2, T>(lowest, highest);
    };

    for (uint8_t corner = 0; corner < 3; corner++) {
        const glm::vec<2, T> edge = triangle[(corner + 1) % 3] - triangle[corner];
        const glm::vec<2, T> axis(-edge.y, edge.x);
        if (glm::dot(axis, axis) <= epsilon_squared) {
            continue;
        }

        const glm::vec<2, T> own = extent_along(axis, triangle);
        const glm::vec<2, T> rest = extent_along(axis, other);
        if (own.y < rest.x || rest.y < own.x) {
            return true;
        }
    }
    return false;
}
} // namespace detail

// Separating axis test over the six edge normals. Triangles that only touch count as overlapping.
template <typename T>
bool triangles_overlap(const radix::geometry::Triangle<2, T> &a, const radix::geometry::Triangle<2, T> &b) {
    return !detail::separates(a, b) && !detail::separates(b, a);
}

template <typename T>
constexpr T epsilon() {
    if constexpr (std::is_same_v<T, float>) {
        return 1e-6f;
    } else if constexpr (std::is_same_v<T, double>) {
        return 1e-12;
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type for epsilon");
    }
}
template <typename T>
constexpr T epsilon_sq() {
    return epsilon<T>() * epsilon<T>();
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                              const glm::vec<3, T> &b,
                              const glm::vec<3, T> &c,
                              const bool normalize = true);
template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                              const std::span<const glm::vec<3, T>> positions,
                              const bool normalize = true);

template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>>& positions);

// Negative where the triangle winds the other way.
template <typename T>
T compute_signed_triangle_area(const glm::vec<2, T> &v0, const glm::vec<2, T> &v1, const glm::vec<2, T> &v2);
template <typename T>
T compute_signed_triangle_area(const std::array<glm::vec<2, T>, 3> &triangle);
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<2, T>> positions);
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<2, T>> &positions);

template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions);

template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2, const T min_area = 2 * epsilon<T>());
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const std::array<glm::vec<n_dims, T>, 3> &triangle, const T min_area = 2 * epsilon<T>());
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions, const T min_area = 2 * epsilon<T>());
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions, const T min_area = 2 * epsilon<T>());

} // namespace geometry

#include "geometry.inl"