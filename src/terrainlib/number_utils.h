#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <libassert/assert.hpp>

inline constexpr uint32_t next_power_of_two(uint32_t n) noexcept {
    DEBUG_ASSERT(n <= (uint32_t{1} << 31));
    return std::bit_ceil(n);
}

inline constexpr uint64_t next_power_of_two(uint64_t n) noexcept {
    DEBUG_ASSERT(n <= (uint64_t{1} << 63));
    return std::bit_ceil(n);
}

inline constexpr uint32_t prev_power_of_two(uint32_t n) noexcept {
    return std::bit_floor(n);
}

inline constexpr uint64_t prev_power_of_two(uint64_t n) noexcept {
    return std::bit_floor(n);
}

template <typename T>
T next_higher(T x) {
    static_assert(std::is_arithmetic_v<T>, "T must be a primitive numeric type");

    if constexpr (std::is_integral_v<T>) {
        if (x == std::numeric_limits<T>::max()) {
            throw std::overflow_error("No higher value exists for this type");
        }
        return x + 1;
    } else {
        return std::nextafter(x, std::numeric_limits<T>::infinity());
    }
}

template <typename T>
T next_lower(T x) {
    static_assert(std::is_arithmetic_v<T>, "T must be a primitive numeric type");

    if constexpr (std::is_integral_v<T>) {
        if (x == std::numeric_limits<T>::lowest()) {
            throw std::underflow_error("No lower value exists for this type");
        }
        return x - 1;
    } else {
        return std::nextafter(x, -std::numeric_limits<T>::infinity());
    }
}
