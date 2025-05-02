#ifndef MESHBUILDING_H
#define MESHBUILDING_H

#include <tl/expected.hpp>

#include "Dataset.h"
#include "srs.h"

#include "mesh/terrain_mesh.h"
#include "border.h"

namespace terrainbuilder::mesh {

enum class BuildError {
    OutOfBounds,
    EmptyRegion
};
std::ostream &operator<<(std::ostream &os, BuildError error);

/// Builds a mesh from the given height dataset.
tl::expected<TerrainMesh, BuildError> build_reference_mesh_patch(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &clip_srs, const radix::geometry::Aabb3d &clip_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds);
}

#endif
