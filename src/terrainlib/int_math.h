#pragma once

#include <concepts>
#include <limits>
#include <optional>

#include <libassert/assert.hpp>

template <std::integral T, std::unsigned_integral Exp>
[[nodiscard]] constexpr T ipow(T base, Exp exp) noexcept {
    T result{1};

    while (exp > 0) {
        if (exp & Exp{1}) {
            result *= base;
        }

        exp >>= 1;

        if (exp > 0) {
            base *= base;
        }
    }

    return result;
}
template <std::integral T, std::unsigned_integral Exp>
[[nodiscard]] constexpr T ipow2(Exp exp) noexcept {
    DEBUG_ASSERT(exp < static_cast<Exp>(std::numeric_limits<T>::digits));
    return static_cast<T>(T{1} << exp);
}
static_assert(ipow2<uint32_t>(0u) == 1);
static_assert(ipow2<uint32_t>(10u) == 1024);
static_assert(ipow(3, 4u) == 81);

template <std::integral T>
[[nodiscard]] constexpr T is_even(T value) noexcept {
    return value % 2 == 0;
}
template <std::integral T>
[[nodiscard]] constexpr T is_odd(T value) noexcept {
    return !is_even(value);
}