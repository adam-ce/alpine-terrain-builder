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
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include <radix/tile.h>

namespace raster_store {

struct StoreTraits {
    using Key = radix::tile::Id;
    using Hasher = Key::Hasher;

    static constexpr unsigned max_zoom_level = std::numeric_limits<uint32_t>::digits;

    static Key root() { return { 0, { 0, 0 } }; }
    static std::optional<Key> parent(const Key& key)
    {
        if (key.zoom_level == 0) {
            return std::nullopt;
        }
        return key.parent();
    }
    static std::optional<std::array<Key, 4>> children(const Key& key)
    {
        if (key.zoom_level >= max_zoom_level) {
            return std::nullopt;
        }
        return key.children();
    }
    static bool is_valid(const Key& key)
    {
        if (key.zoom_level > max_zoom_level) {
            return false;
        }
        if (key.zoom_level == max_zoom_level) {
            return true;
        }
        const uint32_t extent = uint32_t { 1 } << key.zoom_level;
        return key.coords.x < extent && key.coords.y < extent;
    }
    static std::string key_to_string(const Key& key) { return to_string(key); }
};

} // namespace raster_store
