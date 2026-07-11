#pragma once

#include <algorithm>
#include <limits>

#include "number_utils.h"

template <typename T>
struct Range {
    T min;
    T max;

    constexpr Range() : Range(T{0}, T{0}) {}
    constexpr Range(T value) : Range(Range::from_single(value)) {}

    constexpr Range(T min_value, T max_value)
        : min(min_value), max(max_value) {
        this->normalize();
    }

    constexpr void normalize() noexcept {
        if (this->min > this->max) {
            this->max = this->min;
        }
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return this->min == this->max;
    }

    [[nodiscard]] constexpr bool valid() const noexcept {
        return this->min <= this->max;
    }

    [[nodiscard]] constexpr T size() const noexcept {
        return this->max - this->min;
    }

    [[nodiscard]] constexpr bool contains(T value) const noexcept {
        return value >= this->min && value < this->max;
    }

    [[nodiscard]] constexpr bool contains(const Range &other) const noexcept {
        if (other.empty()) {
            return true;
        }

        return other.min >= this->min && other.max <= this->max;
    }

    [[nodiscard]] constexpr bool overlaps(const Range &other) const noexcept {
        if (this->empty() || other.empty()) {
            return false;
        }

        return this->min < other.max && other.min < this->max;
    }

    constexpr void expand(T value) noexcept {
        if (this->empty()) {
            *this = Range::from_single(value);
            return;
        }

        this->min = std::min(this->min, value);
        this->max = std::max(this->max, next_higher(value));
    }

    constexpr void expand(const Range &other) noexcept {
        if (other.empty()) {
            return;
        }

        if (this->empty()) {
            *this = other;
            return;
        }

        this->min = std::min(this->min, other.min);
        this->max = std::max(this->max, other.max);
    }

    constexpr void clamp(const Range &bounds) noexcept {
        if (bounds.empty()) {
            this->min = bounds.min;
            this->max = bounds.max;
            return;
        }

        this->min = std::clamp(this->min, bounds.min, bounds.max);
        this->max = std::clamp(this->max, bounds.min, bounds.max);
        this->normalize();
    }

    constexpr void translate(T offset) noexcept {
        this->min += offset;
        this->max += offset;
    }

    [[nodiscard]] constexpr Range intersection(const Range &other) const noexcept {
        return {
            std::max(this->min, other.min),
            std::min(this->max, other.max)};
    }

    [[nodiscard]] constexpr Range unite(const Range &other) const noexcept {
        if (this->empty()) {
            return other;
        }

        if (other.empty()) {
            return *this;
        }

        return {
            std::min(this->min, other.min),
            std::max(this->max, other.max)};
    }

    [[nodiscard]] static constexpr Range from_single(T value) noexcept {
        return {value, next_higher(value)};
    }

    [[nodiscard]] constexpr bool operator==(const Range &) const noexcept = default;
};

template <typename T>
constexpr Range<T> full_range() {
    return Range<T>(std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max());
}

template <typename T>
constexpr Range<T> empty_range() {
    return Range<T>{};
}
