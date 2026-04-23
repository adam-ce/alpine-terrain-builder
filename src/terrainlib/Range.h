#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <type_traits>

template <typename T>
struct Range {
    T min;
    T max;

    constexpr Range() : min(std::numeric_limits<T>::max()), max(std::numeric_limits<T>::lowest()) {}
    constexpr Range(T min_value, T max_value) : min(min_value), max(max_value) {}

    [[nodiscard]] constexpr bool empty() const noexcept {
        return min >= max;
    }

    [[nodiscard]] constexpr T size() const noexcept {
        static_assert(std::is_arithmetic_v<T>, "Range::size requires arithmetic T");
        return this->empty() ? T{0} : (max - min);
    }

    [[nodiscard]] constexpr bool valid() const noexcept {
        return this->min <= this->max;
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
        if (other.empty()) {
            return false;
        }
        return this->min < other.max && other.min < this->max;
    }

    constexpr void expand(T value) noexcept {
        this->min = std::min(min, value);
        this->max = std::max(max, value + T{1});
    }

    constexpr void expand(const Range &other) noexcept {
        if (other.empty()) {
            return;
        }
        this->min = std::min(this->min, other.min);
        this->max = std::max(this->max, other.max);
    }

    constexpr void clamp(const Range &bounds) noexcept {
        this->min = std::clamp(this->min, bounds.min, bounds.max);
        this->max = std::clamp(this->max, bounds.min, bounds.max);
        if (this->min > this->max) {
            this->min = this->max;
        }
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
        return {value, value + T{1}};
    }

    [[nodiscard]] constexpr bool operator==(const Range &) const noexcept = default;
};
