/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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

#include <optional>

#include <glm/common.hpp>
#include <radix/geometry.h>

#include "containers/Cow.h"
#include "geometry/geometry.h"
#include "mask.h"
#include "mesh/SimpleMesh.h"
#include "mesh/clip.h"
#include "mesh/combine.h"
#include "mesh/texture_trim.h"

inline MeshMask clip_mask_on_bounds(const MeshMask &mask, const radix::geometry::Aabb3d &bounds) {
    MeshMask result;
    result.components.reserve(mask.components.size());

    for (const SimpleMesh &component : mask.components) {
        const Cow<const SimpleMesh> clipped = mesh::clip_on_bounds_and_cap(component, bounds);
        if (!clipped->is_empty()) {
            result.components.push_back(clipped.get());
        }
    }

    return result;
}

inline Cow<const SimpleMesh> clip_on_mask(const SimpleMesh &mesh, const MeshMask &mask, const bool keep_inside = true) {
    if (keep_inside) {
        std::vector<SimpleMesh> clipped_components;
        clipped_components.reserve(mask.components.size());

        for (const SimpleMesh &component : mask.components) {
            const Cow<const SimpleMesh> clipped = mesh::clip_on_mesh(mesh, component, true);
            if (clipped.is_ref()) {
                return Cow<const SimpleMesh>::from_ref(mesh);
            }
            if (!clipped->is_empty()) {
                clipped_components.push_back(clipped.get());
            }
        }

        if (clipped_components.empty()) {
            return Cow<const SimpleMesh>::from_owned(SimpleMesh());
        }

        SimpleMesh result = clipped_components.size() == 1
            ? std::move(clipped_components.front())
            : mesh::combine(clipped_components);
        result.texture = mesh.texture;
        trim_texture_inplace(result);
        return Cow<const SimpleMesh>::from_owned(std::move(result));
    }

    std::optional<SimpleMesh> result;
    const SimpleMesh *current = &mesh;
    for (const SimpleMesh &component : mask.components) {
        const Cow<const SimpleMesh> clipped = mesh::clip_on_mesh(*current, component, false);
        if (!clipped.is_ref()) {
            result = clipped.get();
            current = &result.value();
            if (current->is_empty()) {
                break;
            }
        }
    }

    if (!result.has_value()) {
        return Cow<const SimpleMesh>::from_ref(mesh);
    }

    trim_texture_inplace(result.value());
    return Cow<const SimpleMesh>::from_owned(std::move(result.value()));
}
