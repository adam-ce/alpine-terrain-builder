#pragma once

#include "Bound.h"

struct RangeFull {
    template <typename T>
    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    template <typename T>
    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    template <typename T>
    [[nodiscard]] constexpr bool contains(const T &) const noexcept {
        return true;
    }

    template <typename T>
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return false;
    }

    [[nodiscard]] constexpr bool operator==(const RangeFull &) const noexcept = default;
};
