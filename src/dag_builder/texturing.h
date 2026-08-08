#pragma once

#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "atlas/BakeSource.h"
#include "atlas/bake_cluster_texture.h"
#include "cluster.h"
#include "enumerate.h"
#include "merge/clusters.h"
#include "range_utils.h"
#include "utils.h"
#include "uv/atlas.h"

struct TextureOptions {
    uv::AtlasOptions atlas;
    BakeTextureOptions bake;
    // Inherit a child texture when every source cluster shares it and their uvs agree.
    bool allow_texture_reuse = true;
};

// Unwrap and bake a fresh texture for every merged cluster that cannot inherit one.
[[nodiscard]]
inline Clustering texture_clusters(
    Clustering merged,
    const Clustering &source,
    const std::span<const std::vector<uint32_t>> partition_to_clusters,
    const TextureOptions &options) {
    DEBUG_ASSERT(merged.cluster_count() == partition_to_clusters.size());

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        const std::span<const uint32_t> sources = partition_to_clusters[index];
        // Renamed to needs_rebake once the old unwrap path is gone.
        // TODO: this is fine for now but ion general we dont want to access the detail
        if (!detail::check_merge_needs_unwrap(source, sources, options.allow_texture_reuse)) {
            continue;
        }

        const std::vector<glm::dvec3> positions = merged.get_cluster_positions(index);
        uv::Atlas atlas = uv::build_atlas(cluster.local_triangles, positions, options.atlas);
        // Only when xatlas charted nothing at all, so every triangle was degenerate against its epsilon.
        if (atlas.uvs.empty()) {
            LOG_WARN("Cluster {} could not be unwrapped, leaving it untextured", index);
            continue;
        }

        const glm::uvec2 size = atlas.size;

        cluster = apply_atlas(cluster, std::move(atlas));
        const BakeSource bake_source = collect_bake_source(source, sources);
        cluster.texture_id = merged.textures.add(
            bake_cluster_texture(cluster, merged.positions, bake_source, size, options.bake));
    }

    trim_textures_inplace(merged);
    return merged;
}