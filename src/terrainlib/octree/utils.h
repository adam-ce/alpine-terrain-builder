/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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
#include <vector>

#include "octree/Id.h"

namespace octree {

inline void for_each_descendant_at_level(const Id &node, const Id::Level target_level, const std::function<void(const Id &)> &callback) {
    if (target_level <= node.level()) {
        return;
    }

    const Id::Level level_diff = target_level - node.level();
    const Id::Index start = node.index_on_level() << (3 * level_diff);
    const Id::Index count = Id::Index(1) << (3 * level_diff);

    for (Id::Index i = 0; i < count; ++i) {
        callback(Id(target_level, start + i));
    }
}

inline std::vector<Id> list_descendant_at_level(const Id &node, const Id::Level target_level) {
    if (target_level <= node.level()) {
        return {};
    }

    const Id::Level level_diff = target_level - node.level();
    const Id::Index start = node.index_on_level() << (3 * level_diff);
    const Id::Index count = Id::Index(1) << (3 * level_diff);

    std::vector<Id> nodes;
    nodes.reserve(count);
    for (Id::Index i = 0; i < count; ++i) {
        nodes.emplace_back(target_level, start + i);
    }
    return nodes;
}
}
