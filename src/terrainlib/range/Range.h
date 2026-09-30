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

#include "Bound.h"
#include "RangeBounds.h"
#include "numeric/number_utils.h"

template <typename T>
struct Range : RangeBounds<Range<T>, T> {
    T start;
    T end;

    constexpr Range() : Range({}, {}) {}
    constexpr Range(T value) requires std::integral<T> || std::floating_point<T>
        : Range(value, next_higher(value)) {}
    constexpr Range(T start_value, T end_value) : start(start_value), end(end_value) {}

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::included(this->start);
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::excluded(this->end);
    }

    [[nodiscard]] constexpr T size() const noexcept {
        return this->end - this->start;
    }

    [[nodiscard]] constexpr bool is_in_bounds(const T &len) const noexcept {
        return this->start <= this->end && this->end <= len;
    }

    [[nodiscard]] constexpr bool is_overlapping(const Range &other) const noexcept {
        return this->start < other.end && other.start < this->end;
    }

    [[nodiscard]] constexpr bool operator==(const Range &) const noexcept = default;
};
