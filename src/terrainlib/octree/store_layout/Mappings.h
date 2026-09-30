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

#include <array>
#include <optional>
#include <string_view>

#include "octree/store_layout/Flat.h"
#include "octree/store_layout/LevelAndCoordinateDirectories.h"
#include "store/path_layout.h"

namespace octree::store_layout {

inline store::path_layout::Mapping<Id> flat() { return { "flat", flat_key_to_node_path, flat_node_path_to_key }; }

inline store::path_layout::Mapping<Id> level_and_coordinate_directories()
{
    return {
        "level_and_coordinate_directories",
        level_and_coordinate_key_to_node_path,
        level_and_coordinate_node_path_to_key,
    };
}

inline std::array<store::path_layout::Mapping<Id>, 2> all() { return { flat(), level_and_coordinate_directories() }; }

inline std::optional<store::path_layout::Mapping<Id>> from_id(const std::string_view id)
{
    for (const auto mapping : all()) {
        if (mapping.id == id) {
            return mapping;
        }
    }
    return std::nullopt;
}

} // namespace octree::store_layout
