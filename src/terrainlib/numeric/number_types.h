#pragma once

// -------------- [u]std::int128_t --------------
#include <cstdint>

#if defined(__SIZEOF_INT128__)
#define HAS_INT128
#define HAS_UINT128
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
using int128_t = __int128;
using uint128_t = unsigned __int128;
#pragma GCC diagnostic pop
#endif


// -------------- float32_t & float64_t --------------
#include <climits>

#if defined(__STDCPP_FLOAT32_T__) || defined(__STDCPP_FLOAT64_T__)
#include <stdfloat>
#endif

#if defined(__STDCPP_FLOAT32_T__)
using float32_t = std::float32_t;
#define HAS_FLOAT32
#elif defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__ * CHAR_BIT == 32
using float32_t = float;
#define AS_FLOAT32 = true;
#elif defined(__SIZEOF_DOUBLE__) && __SIZEOF_DOUBLE__ * CHAR_BIT == 32
using float32_t = double;
#define HAS_FLOAT32
#elif defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ * CHAR_BIT == 32
using float32_t = long double;
#define HAS_FLOAT32
#elif defined(_MSC_VER) && CHAR_BIT == 8
using float32_t = float;
#define HAS_FLOAT32
#endif

#if defined(__STDCPP_FLOAT64_T__)
using float64_t = std::float64_t;
#define HAS_FLOAT64
#elif defined(__SIZEOF_DOUBLE__) && __SIZEOF_DOUBLE__ * CHAR_BIT == 64
using float64_t = double;
#define HAS_FLOAT64
#elif defined(__SIZEOF_LONG_DOUBLE__) && __SIZEOF_LONG_DOUBLE__ * CHAR_BIT == 64
using float64_t = long double;
#define HAS_FLOAT64
#elif defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__ * CHAR_BIT == 64
using float64_t = float;
#define HAS_FLOAT64
#elif defined(_MSC_VER) && CHAR_BIT == 8
using float64_t = double;
#define HAS_FLOAT64
#endif


// -------------- integer_for_width --------------
#include "numeric/int_math.h"
#include "numeric/wide_integer.h"

template <std::size_t Bits>
inline constexpr std::size_t storage_bits_v = next_power_of_two(Bits < 8 ? 8 : Bits);

template <std::size_t Width, bool Signed>
struct integer_for_width;

template <bool Signed>
struct integer_for_width<8, Signed> {
    using type = std::conditional_t<Signed, std::int8_t, std::uint8_t>;
    static constexpr bool is_native = true;
};
template <bool Signed>
struct integer_for_width<16, Signed> {
    using type = std::conditional_t<Signed, std::int16_t, std::uint16_t>;
    static constexpr bool is_native = true;
};
template <bool Signed>
struct integer_for_width<32, Signed> {
    using type = std::conditional_t<Signed, std::int32_t, std::uint32_t>;
    static constexpr bool is_native = true;
};
template <bool Signed>
struct integer_for_width<64, Signed> {
    using type = std::conditional_t<Signed, std::int64_t, std::uint64_t>;
    static constexpr bool is_native = true;
};

#if defined(HAS_INT128) && defined(HAS_UINT128)
template <bool Signed>
struct integer_for_width<128, Signed> {
    using type = std::conditional_t<Signed, int128_t, uint128_t>;
    static constexpr bool is_native = true;
};
#else
template <bool Signed>
struct integer_for_width<128, Signed> {
    using type = ::math::wide_integer::uintwide_t<128, std::uint32_t, void, Signed>;
    static constexpr bool is_native = false;
};
#endif

template <std::size_t Width, bool Signed> requires(Width > 128)
struct integer_for_width<Width, Signed> {
    static_assert(is_power_of_two(Width), "wide-integer width must be a power of two");

    using type = ::math::wide_integer::uintwide_t<Width, wide_limb_t, void, Signed>;
    static constexpr bool is_native = false;
};

template <class T>
inline constexpr std::size_t integer_width_v = std::numeric_limits<T>::digits + (std::numeric_limits<T>::is_signed ? 1u : 0u);

template <std::size_t Bits>
struct uint_for_bits {
    using type = typename integer_for_width<storage_bits_v<Bits>, false>::type;
};
template <std::size_t Bits>
using uint_for_bits_t = typename uint_for_bits<Bits>::type;

template <std::size_t Bits>
struct int_for_bits {
    using type = typename integer_for_width<storage_bits_v<Bits>, true>::type;
};
template <std::size_t Bits>
using int_for_bits_t = typename int_for_bits<Bits>::type;
