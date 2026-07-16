#pragma once

#include <cstdint>
#include <optional>

#include "Range.h"
#include "build_config.h"
#include "octree/storage/MeshStorage.h"
#include "storage.h"
#include "uv/unwrap.h"

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
    uv::Algorithm uv_unwrap_algorithm;
    octree::Id root_node = octree::Id::root();
    IncludeMode include_mode = IncludeMode::CurrentOnly;
    bool write_debug_meshes = IS_DEBUG_BUILD;
    bool parallelize = false;
    bool resume = true;
};

void build_full(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options);

void build_levels(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options,
    const AnyRange<uint32_t> &level_range);

} // namespace dag
