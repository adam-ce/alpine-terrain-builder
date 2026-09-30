/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <cmath>
#include <type_traits>

#include <glm/glm.hpp>

template <typename T>
T quantize_round(const T x, const T epsilon) {
    return std::round(x / epsilon) * epsilon;
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize_round(const glm::vec<n_dims, T> &v, const T epsilon) {
    return glm::round(v / epsilon) * epsilon;
}

template <typename T>
int64_t quantize_index(const T x, const T epsilon) {
    if constexpr (std::is_integral_v<T>) {
        if (x < 0) {
            return (x - epsilon + 1) / epsilon;
        }
        return x / epsilon;
    } else {
        return static_cast<int64_t>(std::floor(x / epsilon));
    }
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, int64_t> quantize_index(const glm::vec<n_dims, T> &v, const T epsilon) {
    glm::vec<n_dims, int64_t> result;
    for (glm::length_t i = 0; i < n_dims; i++) {
        result[i] = quantize_index(v[i], epsilon);
    }
    return result;
}

template <typename T>
T quantize_floor(const T x, const T epsilon) {
    if constexpr (std::is_integral_v<T>) {
        if (x < 0) {
            return ((x - epsilon + 1) / epsilon) * epsilon;
        }
        return (x / epsilon) * epsilon;
    } else {
        return std::floor(x / epsilon) * epsilon;
    }
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize_floor(const glm::vec<n_dims, T> &v, const T epsilon) {
    glm::vec<n_dims, T> result;
    for (glm::length_t i = 0; i < n_dims; i++) {
        result[i] = quantize_floor(v[i], epsilon);
    }
    return result;
}
