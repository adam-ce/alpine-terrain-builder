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
#include "io/compression.h"
#include "statistics.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

namespace rf_merger::merge {

struct Options {
    std::filesystem::path left;
    std::filesystem::path right;
    std::filesystem::path priorities;
    std::filesystem::path output;
    unsigned jobs = 1;
    std::optional<std::filesystem::path> cache = std::nullopt;
    io::envelope::CompressionAlgorithm compression_algorithm = io::envelope::CompressionAlgorithm::ZstdDefaultCompression;
};

struct Report {
    statistics::Totals statistics;
    bool statistics_complete = true;
    std::uint64_t restored_tiles = 0;
    unsigned tile_side = 0;
};

// Merges two published RF snapshots into a new published snapshot at
// options.output. Throws Error::Exception for invalid inputs and failures; a
// failure during production retains the incomplete .part snapshot with its
// completed tiles indexed. When stop_requested returns true, active tiles are
// finished and saved, then an Error::Exception with code Cancelled is thrown.
Report run(const Options& options, const std::function<bool()>& stop_requested = {});

} // namespace rf_merger::merge
