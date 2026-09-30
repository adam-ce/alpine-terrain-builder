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
#include "raster_store/Tile.h"
#include "raster_store/attribution.h"
#include "raster_store/pixel.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <radix/tile.h>
#include <string>
#include <variant>
#include <vector>

namespace rf_builder::run {
using Key = radix::tile::Id;
// Checks for cancellation and checkpoints; throws Error::Exception with code
// Cancelled after saving the completed tiles.
using Poll = std::function<void()>;
struct Options {
    std::filesystem::path output;
    unsigned tile_side = 4096;
    unsigned jobs = 1;
    std::optional<std::filesystem::path> cache = std::nullopt;
    std::uint32_t attribution_index = 0;
    std::optional<raster_store::pixel::Mapping> value_mapping = std::nullopt;
};
struct Report {
    std::uint64_t tile_count = 0;
    std::uint64_t tile_bytes = 0;
    std::uint64_t reused_tiles = 0;
    unsigned tile_side = 0;
};
struct Subdivide {
    std::vector<Key> children;
};
template <typename PixelType>
using Prepared = std::variant<std::monostate, raster_store::Tile<PixelType>, Subdivide>;
struct NetworkStats {
    std::uint64_t requests = 0;
    std::uint64_t bytes = 0;
};

// Source callbacks run on the coordinator except prepare, which owns one state
// instance per worker. Source selection/weights never depend on output mutation.
// total, next, initialize and prepare throw Error::Exception on failure.
template <typename PixelType>
struct Source {
    raster_store::attribution::Entity attribution;
    std::function<Expected<void>(const std::filesystem::path&)> validate_cache;
    std::function<Expected<void>(const std::filesystem::path&)> write_inputs;
    std::function<double(const Poll&)> total;
    std::function<std::optional<Key>(const Poll&)> next;
    std::function<void(unsigned, const Poll&)> initialize;
    std::function<Prepared<PixelType>(unsigned, const Key&)> prepare;
    // Present only for online sources. Refine virtual cached ancestors before HTTP.
    std::function<Subdivide(const Key&)> refine_cached;
    std::function<double(const Key&)> weight;
    std::function<NetworkStats()> network_stats;
};

// Throws Error::Exception for invalid attribution indices and tile sizes.
void validate_options(const Options& options);
Expected<raster_store::attribution::Entity> attribution(const Options& options);
Expected<std::string> identifier(const std::string& input);
std::string gdal_identifier(const std::string& input);
Expected<void> check_link_filesystem(const std::filesystem::path& cache, const std::filesystem::path& output);
// Builds and publishes the snapshot at options.output. Throws Error::Exception
// for invalid inputs and failures; a failure during production retains the
// incomplete .part snapshot with its completed tiles indexed. When
// stop_requested returns true, active tiles are finished and saved, then an
// Error::Exception with code Cancelled is thrown.
template <typename PixelType>
Report execute(const Options& options, Source<PixelType> source, const std::function<bool()>& stop_requested = {});
} // namespace rf_builder::run
