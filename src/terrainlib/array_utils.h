#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <type_traits>

template <typename T, size_t N, typename Func>
auto transform_array(const std::array<T, N> &input, Func transform) {
    using Output = std::invoke_result_t<Func, const T &>;
    std::array<Output, N> result{};
    for (size_t i = 0; i < N; i++) {
        result[i] = std::invoke(transform, input[i]);
    }
    return result;
}

template <typename T, size_t N, typename Func>
auto transform_array(std::span<const T, N> input, Func &&transform) {
    static_assert(N != std::dynamic_extent);
    using Output = std::remove_cvref_t<std::invoke_result_t<Func &, const T &>>;

    std::array<Output, N> result{};
    for (size_t i = 0; i < N; ++i) {
        result[i] = std::invoke(transform, input[i]);
    }
    return result;
}
