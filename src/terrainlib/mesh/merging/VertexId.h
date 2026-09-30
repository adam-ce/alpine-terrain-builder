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

#include <cstddef>
#include <cstdint>

#include "hash_utils.h"

namespace mesh::merging {

// Identifies a specific vertex within a particular mesh.
struct VertexId {
    uint32_t mesh_index;
    uint32_t vertex_index;

    auto operator<=>(const VertexId &) const = default;
};

}

template <>
struct std::hash<mesh::merging::VertexId> {
    size_t operator()(const mesh::merging::VertexId &v) const noexcept {
        return ::hash::combine(v.mesh_index, v.vertex_index);
    }
};
