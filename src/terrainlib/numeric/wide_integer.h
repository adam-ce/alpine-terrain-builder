#pragma once

// Must precede the uintwide_t.h include: it is only honoured on the header's first
// inclusion in a translation unit, so nothing else may include it first.
#if defined(__SIZEOF_INT128__)
#define WIDE_INTEGER_HAS_LIMB_TYPE_UINT64
#endif
#include <math/wide_integer/uintwide_t.h>

#include <cstdint>

#if defined(__SIZEOF_INT128__)
using wide_limb_t = std::uint64_t;
#else
using wide_limb_t = std::uint32_t;
#endif

#include <fmt/ostream.h>
template <auto Width, typename LimbType, typename AllocatorType, bool IsSigned>
struct fmt::formatter<::math::wide_integer::uintwide_t<Width, LimbType, AllocatorType, IsSigned>> : fmt::ostream_formatter {};
