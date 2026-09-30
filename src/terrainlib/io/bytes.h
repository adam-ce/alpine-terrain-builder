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
