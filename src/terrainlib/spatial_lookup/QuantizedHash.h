/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include <glm/glm.hpp>

#include "VecHash.h"
#include "numeric/quantize.h"

namespace spatial_lookup {
namespace detail {

template <glm::length_t n_dims, typename T>
struct QuantizedVecHash {
    using Vec = glm::vec<n_dims, T>;

    explicit QuantizedVecHash(T epsilon) : epsilon(epsilon) {}

    T epsilon;

    size_t operator()(const Vec &v) const noexcept {
        const Vec quantized = quantize_floor(v, this->epsilon);
        return VecHash<n_dims, T>{}(quantized);
    }
};

template <glm::length_t n_dims, typename T>
struct QuantizedVecEqual {
    using Vec = glm::vec<n_dims, T>;

    explicit QuantizedVecEqual(T epsilon) : epsilon(epsilon) {}

    T epsilon;
    bool operator()(const Vec &a, const Vec &b) const noexcept {
        return quantize_floor(a, this->epsilon) == quantize_floor(b, this->epsilon);
    }
};

} // namespace detail
} // namespace spatial_lookup
