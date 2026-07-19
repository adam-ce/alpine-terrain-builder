#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include <expected>

#include "io/Error.h"

namespace io {

std::expected<void, Error> write_bytes_to_path(const std::span<const uint8_t> bytes, const std::filesystem::path &path, bool make_dirs = true);
std::expected<std::vector<uint8_t>, Error> read_bytes_from_path(const std::filesystem::path &path);

}
