#ifndef TERRAINBUILDER_H
#define TERRAINBUILDER_H

#include <filesystem>

#include <radix/geometry.h>

#include "Dataset.h"
#include "srs.h"

namespace terrainbuilder {

void build(
    Dataset &dataset,
    const std::optional<std::filesystem::path> texture_base_path,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &tile_srs,
    const OGRSpatialReference &texture_srs,
    const radix::tile::SrsBounds &tile_bounds,
    const std::filesystem::path &output_path);
}

#endif
