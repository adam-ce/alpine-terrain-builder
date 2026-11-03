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
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

#include <expected>

#include "Error.h"

namespace io::byte {

using vector = std::vector<std::byte>;
using span = std::span<const std::byte>;

// Replace writes and syncs <path>.tmp, then renames it over path, so path holds the old or the
// new bytes even after a crash, and hard links to the old file keep the old bytes. Replace writes
// of the same path must not run concurrently.
enum class WriteMode { Overwrite, CreateNew, Replace };

Expected<void> write(span bytes, const std::filesystem::path& path, WriteMode mode, bool make_dirs = true);
// Reads the whole file, or its first max_size bytes if it is longer.
Expected<vector> read(const std::filesystem::path& path, std::size_t max_size = std::numeric_limits<std::size_t>::max());

// Reads byte ranges of a file with pread; const reads may run concurrently. Implemented for Linux only.
class RandomAccessFile {
public:
    static Expected<RandomAccessFile> open(const std::filesystem::path& path);

    RandomAccessFile(RandomAccessFile&& other) noexcept;
    ~RandomAccessFile();

    std::uint64_t size() const { return m_size; }
    // Reads exactly size bytes; fails with CorruptData if the range passes the end of the file.
    Expected<vector> read(std::uint64_t offset, std::size_t size) const;

private:
    RandomAccessFile(int descriptor, std::filesystem::path path);

    int m_descriptor = -1;
    std::uint64_t m_size = 0;
    std::filesystem::path m_path;
};

}
