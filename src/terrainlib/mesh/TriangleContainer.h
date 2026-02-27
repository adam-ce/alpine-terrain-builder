#pragma once

#include <concepts>
#include <cstddef>

#include <glm/glm.hpp>

template <typename T>
concept TriangleContainer = requires(T c) {
    { c.size() } -> std::convertible_to<size_t>;
    { c[0] } -> std::convertible_to<glm::uvec3>;
    { c[1] } -> std::convertible_to<glm::uvec3>;
    { c[2] } -> std::convertible_to<glm::uvec3>;
};
