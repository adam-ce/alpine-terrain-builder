#ifndef TERRAINBUILDER_H
#define TERRAINBUILDER_H

#include <filesystem>

#include <radix/geometry.h>

#include "Dataset.h"
#include "mesh/SimpleMesh.h"

namespace terrainbuilder {

void build_and_save(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const std::optional<std::filesystem::path> texture_base_path,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_path);

SimpleMesh build(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const std::optional<std::filesystem::path> texture_base_path,
    const OGRSpatialReference &mesh_srs);

}

#endif
