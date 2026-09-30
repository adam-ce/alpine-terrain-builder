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

#include "numeric/int_math.h"

template <glm::length_t N, std::integral T>
[[nodiscard]] constexpr glm::vec<N, T> saturating_add(const glm::vec<N, T>& lhs, const glm::vec<N, T>& rhs) noexcept {
    glm::vec<N, T> result;
    for (glm::length_t i = 0; i < N; i++) {
        result[i] = saturating_add(lhs[i], rhs[i]);
    }
    return result;
}

template <glm::length_t N, std::integral T>
[[nodiscard]] constexpr glm::vec<N, T> saturating_sub(const glm::vec<N, T> &lhs, const glm::vec<N, T> &rhs) noexcept {
    glm::vec<N, T> result;
    for (glm::length_t i = 0; i < N; i++) {
        result[i] = saturating_sub(lhs[i], rhs[i]);
    }
    return result;
}
