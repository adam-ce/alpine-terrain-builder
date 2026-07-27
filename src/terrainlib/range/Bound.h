#pragma once

#include <optional>
#include <utility>

enum class BoundKind {
    Included,
    Excluded,
    Unbounded,
};

template <typename T>
struct Bound {
    BoundKind kind;
    std::optional<T> value;

    [[nodiscard]] static constexpr Bound included(T value) noexcept {
        return {BoundKind::Included, std::move(value)};
    }

    [[nodiscard]] static constexpr Bound excluded(T value) noexcept {
        return {BoundKind::Excluded, std::move(value)};
    }

    [[nodiscard]] static constexpr Bound unbounded() noexcept {
        return {BoundKind::Unbounded, std::nullopt};
    }

    [[nodiscard]] constexpr bool operator==(const Bound &) const noexcept = default;
};
