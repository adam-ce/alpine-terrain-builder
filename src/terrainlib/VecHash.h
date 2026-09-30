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

#include <glm/glm.hpp>

#include "hash_utils.h"

template <glm::length_t n_dims, typename T>
struct VecHash {
    using Vec = glm::vec<n_dims, T>;

    size_t operator()(const Vec &v) const noexcept {
        size_t seed = hash::default_seed();
        for (glm::length_t i = 0; i < n_dims; i++) {
            hash::append(seed, v[i]);
        }
        return seed;
    }
};

using DVec3Hash = VecHash<3, double>;
using DVec2Hash = VecHash<2, double>;
using UVec3Hash = VecHash<3, uint32_t>;
