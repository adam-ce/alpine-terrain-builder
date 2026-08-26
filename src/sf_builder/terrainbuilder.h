#pragma once

#include <filesystem>
#include <expected>

#include <radix/geometry.h>

#include "Dataset.h"
#include "octree/Id.h"
#include "mesh/SimpleMesh.h"
#include "tile_provider.h"
#include "mesh/storage.h"
#include "Error.h"

namespace terrainbuilder {

void build_and_save_patch(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_path);

std::optional<SimpleMesh> build_patch(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs);

std::expected<void, ::Error> build_all_patches(
    Dataset &dataset,
    const octree::Id::Level target_level,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_base_path,
    const std::string &output_format,
    const bool overwrite_existing);
}
