#pragma once

#include <concepts>

inline constexpr size_t RUNTIME_TAG = SIZE_MAX;

template <typename T>
concept Size =
    requires(const T& s) {
        { s.value() } noexcept -> std::same_as<size_t>;
        { s.is_runtime() } noexcept -> std::same_as<bool>;
        { s.is_comptime() } noexcept -> std::same_as<bool>;
        { static_cast<size_t>(s) } noexcept -> std::same_as<size_t>;
    };

template <size_t N>
struct ComptimeSize {
    constexpr ComptimeSize() noexcept = default;

    static constexpr size_t value() noexcept {
        return N;
    }
    static constexpr bool is_runtime() noexcept {
        return false;
    }
    static constexpr bool is_comptime() noexcept {
        return true;
    }
    constexpr operator size_t() const noexcept {
        return N;
    }
};

struct RuntimeSize {
    constexpr explicit RuntimeSize(size_t n) noexcept : n(n) {}

    constexpr size_t value() const noexcept {
        return n;
    }
    static constexpr bool is_runtime() noexcept {
        return true;
    }
    static constexpr bool is_comptime() noexcept {
        return false;
    }
    constexpr operator size_t() const noexcept {
        return n;
    }

private:
    size_t n;
};

static_assert(Size<ComptimeSize<42>>);
static_assert(Size<RuntimeSize>);

template <size_t N>
constexpr ComptimeSize<N> make_size() noexcept {
    return ComptimeSize<N>{};
}

constexpr RuntimeSize make_size(const size_t n) noexcept {
    return RuntimeSize(n);
}
