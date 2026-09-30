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

#include <array>
#include <cstddef>
#include <functional>
#include <type_traits>

template <typename T, size_t N, typename Func>
auto transform_array(const std::array<T, N> &input, Func transform) {
    using Output = std::invoke_result_t<Func, const T &>;
    std::array<Output, N> result{};
    for (size_t i = 0; i < N; i++) {
        result[i] = std::invoke(transform, input[i]);
    }
    return result;
}

template <typename T, size_t N, typename Func>
auto transform_array(std::span<const T, N> input, Func &&transform) {
    static_assert(N != std::dynamic_extent);
    using Output = std::remove_cvref_t<std::invoke_result_t<Func &, const T &>>;

    std::array<Output, N> result{};
    for (size_t i = 0; i < N; ++i) {
        result[i] = std::invoke(transform, input[i]);
    }
    return result;
}
