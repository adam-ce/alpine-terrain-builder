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

#include <fmt/format.h>

#include "octree/Id.h"
#include "string_utils.h"

namespace octree::store_layout {

inline std::filesystem::path level_and_coordinate_key_to_node_path(const Id& id)
{
    const Id::Coords coordinates = id.coords();
    return fmt::format("{}/{}/{}/{}", id.level(), coordinates.x, coordinates.y, coordinates.z);
}

inline std::optional<Id> level_and_coordinate_node_path_to_key(const std::filesystem::path& node_path)
{
    if (node_path.empty() || node_path.is_absolute() || std::distance(node_path.begin(), node_path.end()) != 4) {
        return std::nullopt;
    }

    auto part = node_path.begin();
    const auto level = from_chars<Id::Level>(part->string());
    ++part;
    const auto x = from_chars<Id::Coord>(part->string());
    ++part;
    const auto y = from_chars<Id::Coord>(part->string());
    ++part;
    if (part->has_extension()) {
        return std::nullopt;
    }
    const auto z = from_chars<Id::Coord>(part->string());
    if (!level.has_value() || !x.has_value() || !y.has_value() || !z.has_value()) {
        return std::nullopt;
    }
    return Id::try_make(level.value(), { x.value(), y.value(), z.value() });
}

} // namespace octree::store_layout
