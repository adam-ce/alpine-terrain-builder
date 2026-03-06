#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"

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
