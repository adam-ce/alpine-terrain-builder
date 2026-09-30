/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2021 Adam Celarek-Litofcenko
 * Copyright (C) 2021 Martin Braunsperger
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

#include <expected>

#include "Dataset.h"
#include "srs.h"

#include "mesh/SimpleMesh.h"
#include "border.h"

namespace terrainbuilder {

enum class BuildMeshError {
    OutOfBounds,
    EmptyRegion
};
std::ostream &operator<<(std::ostream &os, BuildMeshError error);

/// Builds a mesh from the given height dataset.
std::expected<SimpleMesh, BuildMeshError> build_reference_mesh_patch(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &clip_srs, const radix::geometry::Aabb3d &clip_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds);
}

