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

// Parses a strict JSON array of attribution indices, highest priority first.
Expected<std::vector<std::uint16_t>> parse(std::string_view json);
Expected<std::vector<std::uint16_t>> read(const std::filesystem::path& path);
Ranks ranks(const std::vector<std::uint16_t>& priorities);

} // namespace rf_merger::priorities
