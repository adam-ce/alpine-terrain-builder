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

#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "containers/Cow.h"

namespace mesh {

Cow<const SimpleMesh> clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds);
Cow<const SimpleMesh> clip_on_bounds_and_cap(
    const SimpleMesh &mesh,
    const radix::geometry::Aabb3d &bounds,
    const bool remesh_planar_patches = true);
Cow<const SimpleMesh> clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh& clip_mesh, const bool keep_inside = true);

}
