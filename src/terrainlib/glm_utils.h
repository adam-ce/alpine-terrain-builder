#pragma once

#include <span>

#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>

template <glm::length_t L, typename T, glm::qualifier Q>
auto iterate(glm::vec<L, T, Q> &vector) {
    return std::span<T, L>(glm::value_ptr(vector), L);
}
template <glm::length_t L, typename T, glm::qualifier Q>
auto iterate(const glm::vec<L, T, Q> &vector) {
    return std::span<const T, L>(glm::value_ptr(vector), L);
}

template <typename TIndex = uint8_t, glm::length_t L, typename T, glm::qualifier Q>
auto enumerate(glm::vec<L, T, Q> &vector) {
    return enumerate<TIndex, std::span<T, L>>(iterate(vector));
}
template <typename TIndex = uint8_t, glm::length_t L, typename T, glm::qualifier Q>
auto enumerate(const glm::vec<L, T, Q> &vector) {
    return enumerate<TIndex, std::span<const T, L>>(iterate(vector));
}
