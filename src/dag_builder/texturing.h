#pragma once

#include <algorithm>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "atlas/BakeSource.h"
#include "atlas/bake_cluster_texture.h"
#include "cluster.h"
#include "enumerate.h"
#include "log.h"
#include "Partitioning.h"
#include "range_utils.h"
#include "texture_sizing.h"
#include "utils.h"
#include "uv/atlas.h"

// Whether any of the clusters carries a texture.
inline bool any_source_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    return std::ranges::any_of(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].has_texture();
    });
}

struct TextureOptions {
    uv::AtlasOptions atlas = {};
    BakeTextureOptions bake = {};
    TextureSizingOptions sizing = {};
};

// Unwrap and bake a texture for every merged cluster whose sources had one.
[[nodiscard]]
inline Clustering texture_clusters(
    Clustering merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const TextureOptions &options) {
    DEBUG_ASSERT(merged.cluster_count() == partition_to_clusters.segment_count());

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        const std::span<const uint32_t> cluster_indices = partition_to_clusters.segment(index);
        if (!any_source_texture(source, cluster_indices)) {
            continue;
        }
        const std::vector<glm::dvec3> positions = merged.get_cluster_positions(index);
        uv::Atlas atlas = uv::build_atlas(cluster.local_triangles, positions, options.atlas);
        // Only when xatlas charted nothing at all, so every triangle was degenerate against its epsilon.
        if (atlas.uvs.empty()) {
            LOG_WARN("Cluster {} could not be unwrapped, leaving it untextured", index);
            continue;
        }

        // The texture is sized for the detail the sources carried, not for the atlas the
        // packer happened to produce.
        const double source_density = compute_source_texel_density(source, cluster_indices);
        const double demanded_texels =
            compute_target_texel_density(options.sizing, source_density) * cluster.triangle_count();
        const double aspect = atlas.aspect();

        cluster = apply_atlas(cluster, std::move(atlas));

        const TextureDemand demand{.texels = demanded_texels, .coverage = compute_utilization(cluster)};
        const glm::uvec2 size = compute_target_size(demand, aspect);
        const BakeSource bake_source = collect_bake_source(source, cluster_indices);
        cluster.texture_id = merged.textures.add(
            bake_cluster_texture(cluster, merged.positions, bake_source, size, options.bake));
    }

    trim_textures_inplace(merged);
    return merged;
}