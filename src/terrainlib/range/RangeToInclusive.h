#pragma once

#include "Bound.h"
#include "RangeBounds.h"

template <typename T>
struct RangeToInclusive : RangeBounds<RangeToInclusive<T>, T> {
    T end;

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::included(this->end);
    }

    [[nodiscard]] constexpr bool operator==(const RangeToInclusive &) const noexcept = default;
};
