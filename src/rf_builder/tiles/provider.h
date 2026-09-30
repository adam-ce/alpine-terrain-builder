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
#include <filesystem>
#include <radix/tile.h>
#include <string>

namespace rf_builder::tiles::provider {
enum class YDirection : std::uint8_t { Down, Up };
struct Settings {
    std::string url_pattern;
    YDirection y_direction = YDirection::Down;
    unsigned min_zoom = 0;
    unsigned max_zoom = 0;
    unsigned tile_size = 0;
    bool operator==(const Settings&) const = default;
};
Expected<Settings> parse(const std::string& json);
Expected<Settings> read(const std::filesystem::path& path);
Expected<unsigned> zoom_offset(const Settings& settings, unsigned output_side);
std::string url(const Settings& settings, const radix::tile::Id& key);
} // namespace rf_builder::tiles::provider
