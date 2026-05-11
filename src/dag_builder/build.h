#pragma once

#include <cstdint>

#include "Range.h"
#include "octree/Id.h"
#include "octree/storage/MeshStorage.h"
#include "storage.h"
#include "uv/unwrap.h"

namespace dag {

struct BuildOptions {
    uint32_t clusters_per_partition;
    float target_ratio;
    uv::Algorithm uv_unwrap_algorithm;
    bool write_debug_meshes;
};

void build_full(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options);

void build_leaves(
    const octree::IndexedMeshStorage &input_storage,
    octree::DagStorage &output_storage,
    const octree::Id &root_node,
    const bool resume = true);

void build_full_inner(
    octree::IndexedDagStorage &storage,
    const BuildOptions &options);

void build_inner_level(
    octree::IndexedDagStorage &storage,
    const octree::Id &root_node,
    const uint32_t &level,
    const BuildOptions &options);

} // namespace dag