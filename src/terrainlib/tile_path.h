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
#include <string>

#include <radix/tile.h>

[[nodiscard]] inline std::filesystem::path google_tile_path(
    const std::filesystem::path& base_path, const radix::tile::Id& tile_id, const std::string& extension)
{
    return base_path / std::to_string(tile_id.zoom_level) / std::to_string(tile_id.coords.x) / (std::to_string(tile_id.coords.y) + extension);
}
