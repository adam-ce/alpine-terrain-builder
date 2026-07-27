#pragma once

#include "Bound.h"
#include "RangeBounds.h"

template <typename T>
struct RangeFrom : RangeBounds<RangeFrom<T>, T> {
    T start;

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::included(this->start);
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    [[nodiscard]] constexpr bool operator==(const RangeFrom &) const noexcept = default;
};
