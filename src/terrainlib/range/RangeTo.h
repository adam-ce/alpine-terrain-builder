#pragma once

#include "Bound.h"
#include "RangeBounds.h"

template <typename T>
struct RangeTo : RangeBounds<RangeTo<T>, T> {
    T end;

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::excluded(this->end);
    }

    [[nodiscard]] constexpr bool operator==(const RangeTo &) const noexcept = default;
};
