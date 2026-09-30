/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <cstdint>

#include "octree/Id.h"
#include "hash_utils.h"

namespace dag {

// A reference to a cluster within a clustered octree node.
struct Id {
    octree::Id source_batch;
    uint32_t cluster_index;

    auto operator<=>(const Id &) const = default;
};

}

template <>
struct std::hash<dag::Id> {
    size_t operator()(const dag::Id &id) const  {
        return ::hash::combine(id.source_batch, id.cluster_index);
    }
};

template <>
struct fmt::formatter<dag::Id> {
    constexpr auto parse(fmt::format_parse_context &ctx) { return ctx.begin(); }
    auto format(const dag::Id &id, fmt::format_context &ctx) const {
        return fmt::format_to(ctx.out(), "{}:{}", id.source_batch, id.cluster_index);
    }
};
