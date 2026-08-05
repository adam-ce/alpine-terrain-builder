#pragma once

#include <tl/expected.hpp>

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
tl::expected<SimpleMesh, BuildMeshError> build_reference_mesh_patch(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &clip_srs, const radix::geometry::Aabb3d &clip_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds);
}

