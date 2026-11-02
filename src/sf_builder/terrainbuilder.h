/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 * Copyright (C) 2024 Adam Celarek-Litofcenko
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
#include <filesystem>
#include <functional>
#include <optional>

#include <radix/geometry.h>

#include "Error.h"
#include "gis/Dataset.h"
#include "inputs.h"
#include "mesh/SimpleMesh.h"
#include "mesh/storage.h"
#include "octree/Id.h"
#include "tile_provider.h"

namespace terrainbuilder {

/// Coarser nodes touch the earth's centre, where coverage in the dataset's SRS is unavailable.
constexpr octree::Id::Level min_target_level = 2;

void build_and_save_patch(
    gis::Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_path);

std::optional<SimpleMesh> build_patch(
    gis::Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const TileProvider *tile_provider,
    const OGRSpatialReference &mesh_srs);

// Builds all nodes at record.target_level into a new attempt and publishes it to
// output_path. Nodes in the cache, an incomplete earlier build with the same record, are
// linked instead of built. When stop_requested returns true, the active nodes are finished
// and the incomplete snapshot is retained, and the result is an error with code Cancelled.
Expected<void> build_all_patches(gis::Dataset& dataset,
    const OGRSpatialReference& texture_srs,
    const TileProvider* tile_provider,
    const OGRSpatialReference& mesh_srs,
    const std::filesystem::path& output_path,
    const inputs::Record& record,
    const std::optional<std::filesystem::path>& cache_path = std::nullopt,
    const std::function<bool()>& stop_requested = {});
}
