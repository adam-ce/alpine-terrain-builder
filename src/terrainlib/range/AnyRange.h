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
