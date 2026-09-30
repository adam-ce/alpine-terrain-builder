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

#include <functional>
#include <string>

#include "octree/Id.h"

namespace octree {

struct StoreTraits {
    using Key = Id;
    using Hasher = std::hash<Key>;

    static constexpr Key root() { return Key::root(); }
    static constexpr auto parent(const Key& key) { return key.parent(); }
    static constexpr auto children(const Key& key) { return key.children(); }
    static constexpr bool is_valid(const Key& key) { return key.level() <= Key::max_level() && key.index_on_level() <= Key::max_index_on_level(key.level()); }
    static std::string key_to_string(const Key& key) { return key.to_string(); }
};

} // namespace octree
