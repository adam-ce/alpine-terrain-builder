#pragma once

#include <cstdint>
#include <optional>
#include <expected>

#include "ContinuationMode.h"
#include "Range.h"
#include "build_config.h"
#include "texturing.h"
#include "octree/storage/MeshStorage.h"
#include "storage.h"
#include "sf/InvalidTopology.h"

namespace dag {

// When building a DAG level, this determines which input nodes are considered.
enum class IncludeMode {
    CurrentOnly,        // when building level L, only include input nodes at exactly level L
    CurrentAndCoarser,  // when building level L, include input nodes at level L and any coarser level L+X
};

struct BuildOptions {
    uint32_t clusters_per_partition;
    std::optional<float> target_ratio;
    std::optional<float> relative_target_error;
    TextureOptions texture_options = {};
    octree::Id root_node = octree::Id::root();
    IncludeMode include_mode = IncludeMode::CurrentOnly;
    bool write_debug_meshes = IS_DEBUG_BUILD;
    bool parallelize = false;
    ContinuationMode continuation_mode = ContinuationMode::Error;
};

std::expected<void, sf::InvalidTopology> build_full(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options);

std::expected<void, sf::InvalidTopology> build_levels(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options,
    const AnyRange<uint32_t> &level_range);

} // namespace dag
