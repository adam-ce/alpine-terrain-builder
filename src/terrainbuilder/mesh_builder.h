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

/// Builds a mesh from the given height dataset.
tl::expected<TerrainMesh, BuildError> build_reference_mesh_tile(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &tile_srs, radix::tile::SrsBounds &tile_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds,
    const Border<int> &vertex_border,
    const bool inclusive_bounds);
}

#endif
