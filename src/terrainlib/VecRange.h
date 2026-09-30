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

#include <concepts>
#include <ranges>
#include <type_traits>

#include <glm/glm.hpp>

template <typename T>
struct IsVec : std::false_type {};

template <glm::length_t n_dims, typename T, glm::qualifier Q>
struct IsVec<glm::vec<n_dims, T, Q>> : std::true_type {};

template <typename T>
inline constexpr bool is_vec_v = IsVec<std::remove_cvref_t<T>>::value;

template <typename Range>
using range_vec_t = std::remove_cvref_t<std::ranges::range_reference_t<Range>>;

template <typename Range>
using range_scalar_t = typename range_vec_t<Range>::value_type;

template <typename Range>
inline constexpr glm::length_t range_dims_v = range_vec_t<Range>::length();

template <typename Range>
concept AnyVecRange =
    std::ranges::input_range<Range> &&
    is_vec_v<std::ranges::range_reference_t<Range>>;

template <typename Range, glm::length_t n_dims, typename T>
concept VecRange =
    std::ranges::input_range<Range> &&
    std::same_as<
        std::remove_cvref_t<std::ranges::range_reference_t<Range>>,
        glm::vec<n_dims, T>>;
