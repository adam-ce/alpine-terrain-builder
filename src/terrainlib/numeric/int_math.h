#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

#include <libassert/assert.hpp>

#include "macros.h"

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

// Smallest integer not below the given real value.
template <std::integral T = uint32_t, std::floating_point F>
[[nodiscard]] T int_ceil(F value) {
    return static_cast<T>(std::ceil(value));
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

namespace detail {
template <std::integral T>
[[nodiscard]] constexpr bool add_overflow_fallback(const T lhs, const T rhs, T &result) noexcept {
    constexpr T minimum = std::numeric_limits<T>::min();
    constexpr T maximum = std::numeric_limits<T>::max();

    if constexpr (std::is_unsigned_v<T>) {
        if (rhs > maximum - lhs) {
            return true;
        }
    } else {
        if (rhs > T{0} && lhs > maximum - rhs) {
            return true;
        }

        if (rhs < T{0} && lhs < minimum - rhs) {
            return true;
        }
    }

    result = static_cast<T>(lhs + rhs);
    return false;
}

template <std::integral T>
[[nodiscard]] constexpr bool sub_overflow_fallback(const T lhs, const T rhs, T &result) noexcept {
    constexpr T minimum = std::numeric_limits<T>::min();
    constexpr T maximum = std::numeric_limits<T>::max();

    if constexpr (std::is_unsigned_v<T>) {
        if (lhs < rhs) {
            return true;
        }
    } else {
        if (rhs > T{0} && lhs < minimum + rhs) {
            return true;
        }

        if (rhs < T{0} && lhs > maximum + rhs) {
            return true;
        }
    }

    result = static_cast<T>(lhs - rhs);
    return false;
}
}

template <std::integral T>
[[nodiscard]] constexpr bool add_overflow(const T lhs, const T rhs, T &result) noexcept {
#if HAS_BUILTIN(__builtin_add_overflow)
    if (!std::is_constant_evaluated()) {
        T temporary;

        if (__builtin_add_overflow(lhs, rhs, &temporary)) {
            return true;
        }

        result = temporary;
        return false;
    }
#endif

    return detail::add_overflow_fallback(lhs, rhs, result);
}

template <std::integral T>
[[nodiscard]]
constexpr bool sub_overflow(const T lhs, const T rhs, T &result) noexcept {
#if HAS_BUILTIN(__builtin_sub_overflow)
    if (!std::is_constant_evaluated()) {
        T temporary;

        if (__builtin_sub_overflow(lhs, rhs, &temporary)) {
            return true;
        }

        result = temporary;
        return false;
    }
#endif

    return detail::sub_overflow_fallback(lhs, rhs, result);
}

template <std::integral T>
[[nodiscard]] constexpr T saturating_add(const T lhs, const T rhs) noexcept {
    T result;

    if (!add_overflow(lhs, rhs, result)) {
        return result;
    }

    if constexpr (std::is_unsigned_v<T>) {
        return std::numeric_limits<T>::max();
    } else {
        return rhs > T{0} ? std::numeric_limits<T>::max() : std::numeric_limits<T>::min();
    }
}

template <std::integral T>
[[nodiscard]] constexpr T saturating_sub(const T lhs, const T rhs) noexcept {
    T result;

    if (!sub_overflow(lhs, rhs, result)) {
        return result;
    }

    if constexpr (std::is_unsigned_v<T>) {
        return T{0};
    } else {
        return rhs > T{0} ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    }
}

[[nodiscard]] inline constexpr uint32_t next_power_of_two(uint32_t n) noexcept {
    DEBUG_ASSERT(n <= (uint32_t{1} << 31));
    return std::bit_ceil(n);
}

[[nodiscard]] inline constexpr uint64_t next_power_of_two(uint64_t n) noexcept {
    DEBUG_ASSERT(n <= (uint64_t{1} << 63));
    return std::bit_ceil(n);
}

[[nodiscard]] inline constexpr uint32_t prev_power_of_two(uint32_t n) noexcept {
    return std::bit_floor(n);
}

[[nodiscard]] inline constexpr uint64_t prev_power_of_two(uint64_t n) noexcept {
    return std::bit_floor(n);
}

[[nodiscard]] inline constexpr uint32_t nearest_power_of_two(uint32_t n) noexcept {
    DEBUG_ASSERT(n <= (uint32_t{1} << 31));
    const uint32_t lower = prev_power_of_two(n);
    if (n == lower) {
        return lower;
    }
    const uint32_t upper = lower * 2;
    return uint64_t{n} * n < uint64_t{lower} * upper ? lower : upper;
}

#if defined(HAS_UINT128)
[[nodiscard]] inline constexpr uint64_t nearest_power_of_two(uint64_t n) noexcept {
    DEBUG_ASSERT(n <= (uint64_t{1} << 63));

    const uint64_t lower = prev_power_of_two(n);
    if (n == lower) {
        return lower;
    }
    const uint64_t upper = lower * 2;
    return uint128_t{n} * n < uint128_t{lower} * upper ? lower : upper;
}
#endif

[[nodiscard]] inline constexpr bool is_power_of_two(uint32_t n) noexcept {
    return std::has_single_bit(n);
}

[[nodiscard]] inline constexpr bool is_power_of_two(uint64_t n) noexcept {
    return std::has_single_bit(n);
}

template <std::unsigned_integral T>
[[nodiscard]] inline constexpr T align_down(T value, T alignment) noexcept {
    return value - value % alignment;
}

template <std::unsigned_integral T>
[[nodiscard]] inline constexpr T align_up(T value, T alignment) noexcept {
    return value + (alignment - value % alignment) % alignment;
}
