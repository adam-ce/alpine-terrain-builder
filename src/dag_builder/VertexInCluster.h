/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include "hash_utils.h"

struct VertexInCluster {
    uint32_t cluster_index;
    uint32_t local_vertex_index;

    auto operator<=>(const VertexInCluster &other) const = default;
};

namespace std {
    template <>
    struct hash<VertexInCluster> {
        size_t operator()(const VertexInCluster &v) const noexcept {
            return ::hash::combine(v.cluster_index, v.local_vertex_index);
        }
    };
}
