#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

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
