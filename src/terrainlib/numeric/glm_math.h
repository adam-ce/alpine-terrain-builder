#pragma once

#include "numeric/int_math.h"

template <glm::length_t N, std::integral T>
[[nodiscard]] constexpr glm::vec<N, T> saturating_add(const glm::vec<N, T>& lhs, const glm::vec<N, T>& rhs) noexcept {
    glm::vec<N, T> result;
    for (glm::length_t i = 0; i < N; i++) {
        result[i] = saturating_add(lhs[i], rhs[i]);
    }
    return result;
}

template <glm::length_t N, std::integral T>
[[nodiscard]] constexpr glm::vec<N, T> saturating_sub(const glm::vec<N, T> &lhs, const glm::vec<N, T> &rhs) noexcept {
    glm::vec<N, T> result;
    for (glm::length_t i = 0; i < N; i++) {
        result[i] = saturating_sub(lhs[i], rhs[i]);
    }
    return result;
}
