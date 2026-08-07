#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <radix/geometry.h>

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

// Positive when b turns counter-clockwise from a, and the signed area of the parallelogram they span.
template <typename T>
T cross_2d(const glm::vec<2, T> &a, const glm::vec<2, T> &b) {
    return a.x * b.y - a.y * b.x;
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
    const T orientation = cross_2d(triangle[1] - triangle[0], triangle[2] - triangle[0]);

    bool inside = orientation != T(0);
    for (uint8_t corner = 0; inside && corner < 3; corner++) {
        const glm::vec<2, T> edge = triangle[(corner + 1) % 3] - triangle[corner];
        inside = cross_2d(edge, point - triangle[corner]) * orientation >= T(0);
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