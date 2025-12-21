#pragma once

constexpr uint32_t next_power_of_two(uint32_t n) noexcept {
    if (n <= 1)
        return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

constexpr uint64_t next_power_of_two(uint64_t n) noexcept {
    if (n <= 1)
        return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}
constexpr size_t next_power_of_two(size_t n) noexcept {
    if constexpr (std::is_same_v<size_t, uint32_t>) {
        return next_power_of_two(n)
    } else if constexpr (std::is_same_v<size_t, uint64_t>) {
        return next_power_of_two(n)
    } else {
        UNREACHABLE();
    }
}
