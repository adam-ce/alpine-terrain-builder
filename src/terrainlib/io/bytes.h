#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include <expected>

#include "Error.h"

namespace io {

Expected<void> write_bytes_to_path(const std::span<const uint8_t> bytes, const std::filesystem::path& path, bool make_dirs = true);
Expected<std::vector<uint8_t>> read_bytes_from_path(const std::filesystem::path& path);

}
