#pragma once

#include <cmath>

#include <glm/glm.hpp>

template <typename T>
T quantize_round(const T x, const T epsilon) {
    return std::round(x / epsilon) * epsilon;
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize_round(const glm::vec<n_dims, T> &v, const T epsilon) {
    return glm::round(v / epsilon) * epsilon;
}

template <typename T>
T quantize_index(const T x, const T epsilon) {
    return static_cast<std::int64_t>(std::floor(x / epsilon));
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, int64_t> quantize_index(const glm::vec<n_dims, T> &v, const T epsilon) {
    return glm::vec<n_dims, int64_t>(glm::floor(v / epsilon));
}

template <typename T>
T quantize_floor(const T x, const T epsilon) {
    return std::floor(x / epsilon) * epsilon;
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize_floor(const glm::vec<n_dims, T> &v, const T epsilon) {
    return glm::floor(v / epsilon) * epsilon;
}
