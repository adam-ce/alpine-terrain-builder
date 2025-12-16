#pragma once

#include <type_traits>

#if defined(__SIZEOF_INT128__)
constexpr bool HAS_INT128 = true;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
using int128_t = __int128;
using uint128_t = unsigned __int128;
#pragma GCC diagnostic pop
#else
constexpr bool HAS_INT128 = false;
#endif

template <typename T>
struct next_precision {
    using type = T;
};

// floats
template <>
struct next_precision<float> {
    using type = double;
};
template <>
struct next_precision<double> {
    using type = long double;
};
template <>
struct next_precision<long double> {
    using type = long double;
};

// signed integers
template <>
struct next_precision<std::int8_t> {
    using type = std::int16_t;
};
template <>
struct next_precision<std::int16_t> {
    using type = std::int32_t;
};
template <>
struct next_precision<std::int32_t> {
    using type = std::int64_t;
};
template <>
struct next_precision<std::int64_t> {
    using type = typename std::conditional<HAS_INT128, int128_t, std::int64_t>::type;
};
#if HAS_INT128
template <>
struct next_precision<int128_t> {
    using type = int128_t;
};
#endif

// unsigned integers
template <>
struct next_precision<std::uint8_t> {
    using type = std::uint16_t;
};
template <>
struct next_precision<std::uint16_t> {
    using type = std::uint32_t;
};
template <>
struct next_precision<std::uint32_t> {
    using type = std::uint64_t;
};
template <>
struct next_precision<std::uint64_t> {
    using type = typename std::conditional<HAS_INT128, uint128_t, std::uint64_t>::type;
};
#if HAS_INT128
template <>
struct next_precision<uint128_t> {
    using type = uint128_t;
};
#endif

// Convenience alias
template <typename T>
using next_precision_t = typename next_precision<T>::type;
