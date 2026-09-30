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
#include <optional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "octree/Id.h"
#include "string_utils.h"

namespace octree::store_layout {

inline std::filesystem::path flat_key_to_node_path(const Id& id) { return fmt::format("{}-{}", id.level(), id.index_on_level()); }

inline std::optional<Id> flat_node_path_to_key(const std::filesystem::path& node_path)
{
    if (node_path.empty() || node_path.is_absolute() || node_path.has_parent_path() || node_path.has_extension()) {
        return std::nullopt;
    }

    const std::string text = node_path.string();
    const size_t separator = text.find('-');
    if (separator == std::string::npos || separator != text.rfind('-')) {
        return std::nullopt;
    }
    const auto level = from_chars<Id::Level>(std::string_view(text).substr(0, separator));
    const auto index = from_chars<Id::Index>(std::string_view(text).substr(separator + 1));
    if (!level.has_value() || !index.has_value()) {
        return std::nullopt;
    }
    return Id::try_make(level.value(), index.value());
}

} // namespace octree::store_layout
