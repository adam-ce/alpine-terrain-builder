#pragma once

#include <array>
#include <span>

#include <glm/glm.hpp>

enum class WindingOrder {
    Clockwise,
    CounterClockwise,
    Degenerate
};

template <typename T>
WindingOrder get_winding_order(const glm::vec<2, T> &a,
                               const glm::vec<2, T> &b,
                               const glm::vec<2, T> &c) {
    const T signed_area2 =
        (b.x - a.x) * (c.y - a.y) -
        (c.x - a.x) * (b.y - a.y);

    if (signed_area2 > T(0)) {
        return WindingOrder::CounterClockwise;
    }
    if (signed_area2 < T(0)) {
        return WindingOrder::Clockwise;
    }
    return WindingOrder::Degenerate;
}

template <typename T>
WindingOrder get_winding_order(const glm::uvec3 &triangle,
                               const std::span<const glm::vec<2, T>> positions) {
    const auto &a = positions[triangle[0]];
    const auto &b = positions[triangle[1]];
    const auto &c = positions[triangle[2]];
    return get_winding_order<T>(a, b, c);
}

inline glm::uvec3 swap_orientation(glm::uvec3 triangle) {
    std::swap(triangle[1], triangle[2]);
    return triangle;
}

template <typename Vertex>
std::array<Vertex, 3> swap_orientation(std::array<Vertex, 3> triangle) {
    std::swap(triangle[1], triangle[2]);
    return triangle;
}

// The triangle wound counter-clockwise. A degenerate one is returned as it is.
template <typename T>
std::array<glm::vec<2, T>, 3> as_counter_clockwise(std::array<glm::vec<2, T>, 3> triangle) {
    if (get_winding_order(triangle[0], triangle[1], triangle[2]) == WindingOrder::Clockwise) {
        return swap_orientation(triangle);
    }
    return triangle;
}
