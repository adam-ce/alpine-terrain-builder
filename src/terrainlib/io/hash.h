/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace io::hash {

enum class Algorithm : std::uint8_t {
    None,
    Xxh3_64,
};

// Returns no bytes for None and the 8 bytes of XXH3-64 with seed 0 in big-endian order, the canonical
// xxHash form, for Xxh3_64. The algorithm must be an enumerator.
std::vector<std::byte> data(std::span<const std::byte> bytes, Algorithm algorithm);

} // namespace io::hash
