#pragma once

#include <concepts>

#include "Bound.h"
#include "RangeBounds.h"
#include "number_utils.h"

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
