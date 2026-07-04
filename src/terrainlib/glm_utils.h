#pragma once

#include <span>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <libassert/assert.hpp>

#include "enumerate.h"

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline T *value_ptr(glm::vec<L, T, Q>& v) noexcept {
    if constexpr (L == 1) {
        return &v.x;
    } else {
        return glm::value_ptr(v);
    }
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline const T *value_ptr(const glm::vec<L, T, Q>& v) noexcept {
    if constexpr (L == 1) {
        return &v.x;
    } else {
        return glm::value_ptr(v);
    }
}

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline T *value_ptr(const std::span<glm::vec<L, T, Q>> v) noexcept {
    if (v.empty()) {
        return nullptr;
    }
    glm::vec<L, T, Q> &begin = *v.data();
    return value_ptr(begin);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline const T *value_ptr(const std::span<const glm::vec<L, T, Q>> v) noexcept {
    if (v.empty()) {
        return nullptr;
    }
    const glm::vec<L, T, Q> &begin = *v.data();
    return value_ptr(begin);
}

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<T, L> as_span(glm::vec<L, T, Q>& v) {
    static_assert(std::is_standard_layout_v<glm::vec<L, T, Q>>);
    static_assert(sizeof(glm::vec<L, T, Q>) == sizeof(T) * L);
    
    return std::span<T, L>(value_ptr(v), L);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<const T, L> as_span(const glm::vec<L, T, Q>& v) {
    static_assert(std::is_standard_layout_v<glm::vec<L, T, Q>>);
    static_assert(sizeof(glm::vec<L, T, Q>) == sizeof(T) * L);

    return std::span<const T, L>(value_ptr(v), L);
}

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<T> flatten(const std::span<glm::vec<L, T, Q>> v) {
    static_assert(std::is_standard_layout_v<glm::vec<L, T, Q>>);
    static_assert(sizeof(glm::vec<L, T, Q>) == sizeof(T) * L);

    return std::span<T>(value_ptr(v), v.size() * L);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<const T> flatten(const std::span<const glm::vec<L, T, Q>> v) {
    static_assert(std::is_standard_layout_v<glm::vec<L, T, Q>>);
    static_assert(sizeof(glm::vec<L, T, Q>) == sizeof(T) * L);

    return std::span<const T>(value_ptr(v), v.size() * L);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<T> flatten(std::vector<glm::vec<L, T, Q>> &v) {
    return flatten(std::span(v));
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<const T> flatten(const std::vector<glm::vec<L, T, Q>> &v) {
    return flatten(std::span(v));
}

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<const glm::vec<L, T, Q>> unflatten(const std::span<const T> v) {
    static_assert(std::is_standard_layout_v<glm::vec<L, T, Q>>);
    static_assert(sizeof(glm::vec<L, T, Q>) == sizeof(T) * L);

    DEBUG_ASSERT(v.size() % L == 0);

    return std::span<const glm::vec<L, T, Q>>(
        reinterpret_cast<const glm::vec<L, T, Q> *>(v.data()),
        v.size() / L);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<glm::vec<L, T, Q>> unflatten(const std::span<T> v) {
    static_assert(std::is_standard_layout_v<glm::vec<L, T, Q>>);
    static_assert(sizeof(glm::vec<L, T, Q>) == sizeof(T) * L);

    DEBUG_ASSERT(v.size() % L == 0);

    return std::span<glm::vec<L, T, Q>>(
        reinterpret_cast<glm::vec<L, T, Q> *>(v.data()),
        v.size() / L);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<const glm::vec<L, T, Q>> unflatten(const std::vector<T> &v) {
    return unflatten<L, T, Q>(std::span(v));
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
inline std::span<glm::vec<L, T, Q>> unflatten(std::vector<T> &v) {
    return unflatten<L, T, Q>(std::span(v));
}

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
auto iterate(glm::vec<L, T, Q> &v) {
    return as_span<L, T, Q>(v);
}
template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
auto iterate(const glm::vec<L, T, Q> &v) {
    return as_span<L, T, Q>(v);
}

template <typename TIndex = uint8_t, glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
auto enumerate(glm::vec<L, T, Q> &vector) {
    return ::enumerate<TIndex, std::span<T, L>>(iterate<L, T, Q>(vector));
}
template <typename TIndex = uint8_t, glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
auto enumerate(const glm::vec<L, T, Q> &vector) {
    return ::enumerate<TIndex, std::span<const T, L>>(iterate<L, T, Q>(vector));
}