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

#include "Error.h"
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace rf_merger::priorities {

// Ranks for all attribution indices: 0 is unattributed, 1 is any unlisted
// nonzero index, and listed indices rank above them, highest first.
using Ranks = std::vector<std::uint32_t>;

// Parses a JSON array of attribution indices, highest priority first.
Expected<std::vector<std::uint16_t>> parse(std::string_view json);
Expected<std::vector<std::uint16_t>> read(const std::filesystem::path& path);
Ranks ranks(const std::vector<std::uint16_t>& priorities);

} // namespace rf_merger::priorities
