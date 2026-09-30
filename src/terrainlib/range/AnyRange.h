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
#include <utility>

#include "Bound.h"
#include "Range.h"
#include "RangeBounds.h"
#include "RangeFull.h"

template <typename T>
struct AnyRange : RangeBounds<AnyRange<T>, T> {
    Bound<T> start;
    Bound<T> end;

    constexpr AnyRange(Bound<T> start_bound, Bound<T> end_bound) noexcept
        : start(std::move(start_bound)), end(std::move(end_bound)) {}

    constexpr AnyRange(std::pair<Bound<T>, Bound<T>> bounds) noexcept
        : start(std::move(bounds.first)), end(std::move(bounds.second)) {}

    template <typename Derived>
    constexpr AnyRange(const RangeBounds<Derived, T> &range) noexcept
        : start(static_cast<const Derived &>(range).start_bound()),
          end(static_cast<const Derived &>(range).end_bound()) {}

    constexpr AnyRange(RangeFull range) noexcept
        : start(range.template start_bound<T>()), end(range.template end_bound<T>()) {}

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return this->start;
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return this->end;
    }

    // Resolves an Unbounded start to 0 and an Unbounded end to len, mirroring how Rust
    // resolves RangeFull/RangeTo/RangeFrom against a slice's length.
    [[nodiscard]] constexpr Range<T> to_range(T len) const noexcept
        requires std::unsigned_integral<T>
    {
        const T range_start = this->start.kind == BoundKind::Unbounded ? T{0} : *this->start.value;
        const T range_end = this->end.kind == BoundKind::Unbounded
            ? len
            : (this->end.kind == BoundKind::Included ? *this->end.value + 1 : *this->end.value);
        return {range_start, range_end};
    }
};
