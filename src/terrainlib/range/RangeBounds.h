#pragma once

#include <utility>

#include "Bound.h"

struct RangeFull;

template <typename T>
struct AnyRange;

template <typename T>
[[nodiscard]] constexpr Bound<T> tighter_start_bound(const Bound<T> &a, const Bound<T> &b) noexcept {
    if (a.kind == BoundKind::Unbounded) {
        return b;
    }
    if (b.kind == BoundKind::Unbounded) {
        return a;
    }
    if (*a.value != *b.value) {
        return *a.value > *b.value ? a : b;
    }
    return a.kind == BoundKind::Excluded ? a : b;
}

template <typename T>
[[nodiscard]] constexpr Bound<T> tighter_end_bound(const Bound<T> &a, const Bound<T> &b) noexcept {
    if (a.kind == BoundKind::Unbounded) {
        return b;
    }
    if (b.kind == BoundKind::Unbounded) {
        return a;
    }
    if (*a.value != *b.value) {
        return *a.value < *b.value ? a : b;
    }
    return a.kind == BoundKind::Excluded ? a : b;
}

template <typename Derived, typename T>
struct RangeBounds {
    [[nodiscard]] constexpr bool contains(const T &item) const noexcept {
        const auto &self = static_cast<const Derived &>(*this);
        const Bound<T> start = self.start_bound();
        const Bound<T> end = self.end_bound();

        const bool after_start = start.kind == BoundKind::Unbounded ||
            (start.kind == BoundKind::Included ? *start.value <= item : *start.value < item);
        const bool before_end = end.kind == BoundKind::Unbounded ||
            (end.kind == BoundKind::Included ? item <= *end.value : item < *end.value);

        return after_start && before_end;
    }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        const auto &self = static_cast<const Derived &>(*this);
        const Bound<T> start = self.start_bound();
        const Bound<T> end = self.end_bound();

        if (start.kind == BoundKind::Unbounded || end.kind == BoundKind::Unbounded) {
            return false;
        }

        return end.kind == BoundKind::Included ? !(*start.value <= *end.value) : !(*start.value < *end.value);
    }

    template <typename OtherDerived>
    [[nodiscard]] constexpr AnyRange<T> intersect(const RangeBounds<OtherDerived, T> &other) const noexcept {
        const auto &self = static_cast<const Derived &>(*this);
        const auto &other_self = static_cast<const OtherDerived &>(other);
        return AnyRange<T>(
            tighter_start_bound(self.start_bound(), other_self.start_bound()),
            tighter_end_bound(self.end_bound(), other_self.end_bound()));
    }

    [[nodiscard]] constexpr AnyRange<T> intersect(const RangeFull &) const noexcept {
        const auto &self = static_cast<const Derived &>(*this);
        return AnyRange<T>(self.start_bound(), self.end_bound());
    }

    [[nodiscard]] constexpr bool operator==(const RangeBounds &) const noexcept = default;
};
