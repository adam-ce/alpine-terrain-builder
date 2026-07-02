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
