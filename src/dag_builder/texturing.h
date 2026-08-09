#pragma once

#include <algorithm>
#include <cmath>
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

// Charts are packed for roughly this much of their atlas.
inline constexpr double ESTIMATED_ATLAS_COVERAGE = 0.75;

// Resolution to pack an atlas at, so its gutter lands near the scale the bake uses.
[[nodiscard]]
inline uint32_t compute_unwrap_resolution(const double demand) {
    return std::max(int_ceil(std::sqrt(demand / ESTIMATED_ATLAS_COVERAGE)), 1u);
}

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

// Unwrap every cluster whose sources carry a texture, and record the size it asks for.
[[nodiscard]]
inline std::vector<PlannedTexture> plan_node_textures(
    Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const TextureOptions &options) {
    std::vector<PlannedTexture> plans;

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        const std::span<const uint32_t> cluster_indices = partition_to_clusters.segment(index);

        // skip clusters having no texture
        if (!any_source_texture(source, cluster_indices)) {
            continue;
        }

        // Perform unwrap at approximately the final resolution
        const double source_density = compute_source_texel_density(source, cluster_indices);
        const double demanded_texels = compute_target_texel_density(options.sizing, source_density) * cluster.triangle_count();
        const std::vector<glm::dvec3> positions = merged.get_cluster_positions(index);
        uv::Atlas atlas = uv::build_atlas(cluster.local_triangles, positions, compute_unwrap_resolution(demanded_texels), options.atlas);
        if (atlas.uvs.empty()) {
            LOG_WARN("Cluster {} could not be unwrapped, leaving it untextured", index);
            continue;
        }
        const double aspect = atlas.aspect();

        // Apply new uvs and duplicated seam vertices
        cluster = apply_atlas(cluster, std::move(atlas));

        // Record the size it asks for, before the node budget has its say
        const TextureDemand demand{
            .texels = demanded_texels,
            .coverage = compute_utilization(cluster)
        };
        plans.push_back(PlannedTexture{
            .cluster_index = index,
            .size = compute_target_size(demand, aspect)
        });
    }

    return plans;
}

// Bake every planned texture at the size the node budget left it.
inline void bake_node_textures(
    Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const std::span<const PlannedTexture> plans,
    const BakeTextureOptions &options) {
    for (const PlannedTexture &planned : plans) {
        const std::span<const uint32_t> cluster_indices = partition_to_clusters.segment(planned.cluster_index);
        Cluster &cluster = merged.clusters[planned.cluster_index];
        const BakeSource bake_source = collect_bake_source(source, cluster_indices);
        const cv::Mat baked = bake_cluster_texture(cluster, merged.positions, bake_source, planned.size, options);
        cluster.texture_id = merged.textures.add(baked);
    }
}

// Unwrap and bake a texture for every merged cluster whose sources had one.
[[nodiscard]]
inline Clustering texture_clusters(
    Clustering merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const TextureOptions &options) {
    DEBUG_ASSERT(merged.cluster_count() == partition_to_clusters.segment_count());

    // Compute the requested texture sizes
    std::vector<PlannedTexture> plans = plan_node_textures(merged, source, partition_to_clusters, options);

    // Fit to node-wide texture budget
    rescale_to_fit_budget(plans, options.sizing.max_node_texels);

    // Render at new size
    bake_node_textures(merged, source, partition_to_clusters, plans, options.bake);

    trim_textures_inplace(merged);
    return merged;
}
