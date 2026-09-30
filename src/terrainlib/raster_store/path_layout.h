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

#include <filesystem>
#include <iterator>
#include <optional>
#include <string>

#include "raster_store/StoreTraits.h"
#include "store/path_layout.h"
#include "string_utils.h"

namespace raster_store::path_layout::zoom_xy_google {

inline std::filesystem::path key_to_node_path(const radix::tile::Id& key)
{
    return std::to_string(key.zoom_level) + "/" + std::to_string(key.coords.x) + "/" + std::to_string(key.coords.y);
}

inline std::optional<radix::tile::Id> node_path_to_key(const std::filesystem::path& path)
{
    if (path.is_absolute() || std::distance(path.begin(), path.end()) != 3) {
        return std::nullopt;
    }
    auto part = path.begin();
    const auto zoom = from_chars<std::uint32_t>(part->string());
    ++part;
    const auto x = from_chars<std::uint32_t>(part->string());
    ++part;
    const auto y = from_chars<std::uint32_t>(part->string());
    if (!zoom || !x || !y) {
        return std::nullopt;
    }
    const radix::tile::Id key { *zoom, { *x, *y } };
    return StoreTraits::is_valid(key) ? std::optional(key) : std::nullopt;
}

inline store::path_layout::Mapping<radix::tile::Id> zoom_x_y_google()
{
    return { "zoom/x/y_google", key_to_node_path, node_path_to_key };
}

inline std::optional<store::path_layout::Mapping<radix::tile::Id>> from_id(const std::string_view id)
{
    if (id == zoom_x_y_google().id) {
        return zoom_x_y_google();
    }
    return std::nullopt;
}

} // namespace raster_store::path_layout::zoom_xy_google
