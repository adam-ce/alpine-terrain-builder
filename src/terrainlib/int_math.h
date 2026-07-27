#pragma once

#include <concepts>
#include <limits>
#include <optional>
#include <type_traits>

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
[[nodiscard]] constexpr bool is_even(T value) noexcept {
    return value % 2 == 0;
}
template <std::integral T>
[[nodiscard]] constexpr bool is_odd(T value) noexcept {
    return !is_even(value);
}

template <std::integral A, std::integral B>
    requires(std::is_signed_v<A> == std::is_signed_v<B>)
[[nodiscard]] constexpr auto int_div_ceil(A a, B b) {
    using T = std::common_type_t<A, B>;
    ASSERT(b != 0);

    const T a_t = static_cast<T>(a);
    const T b_t = static_cast<T>(b);

    if constexpr (std::unsigned_integral<T>) {
        return (a_t + b_t - 1) / b_t;
    } else {
        const T q = a_t / b_t;
        const T r = a_t % b_t;

        // Exact division -> already the ceiling
        if (r == 0) {
            return q;
        }

        // Same sign
        if ((a_t > 0 && b_t > 0) || (a_t < 0 && b_t < 0)) {
            return q + 1;
        }

        // Opposite signs -> truncation toward zero already gave the ceiling
        return q;
    }
}
