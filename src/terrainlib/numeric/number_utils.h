/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
