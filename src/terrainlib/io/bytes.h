/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

#include <expected>

#include "Error.h"

namespace io {

enum class WriteMode { Overwrite, CreateNew };

Expected<void> write_bytes_to_path(const std::span<const std::byte> bytes, const std::filesystem::path& path, bool make_dirs = true);
Expected<void> write_bytes_to_path(std::span<const std::byte> bytes, const std::filesystem::path& path, WriteMode mode, bool make_dirs = true);
// Reads the whole file, or its first max_size bytes if it is longer.
Expected<std::vector<std::byte>> read_bytes_from_path(const std::filesystem::path& path, std::size_t max_size = std::numeric_limits<std::size_t>::max());

}
