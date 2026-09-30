/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
