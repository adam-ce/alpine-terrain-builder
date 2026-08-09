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

enum class ChartingMode {
    PerCluster, // one atlas per cluster
    PerNode, // one atlas over the full clustering
};

struct TextureOptions {
    uv::AtlasOptions atlas = {};
    BakeTextureOptions bake = {};
    TextureSizingOptions sizing = {};
    ChartingMode charting = ChartingMode::PerCluster;
};

// Texels each cluster should carry, from how densely its sources were textured.
[[nodiscard]]
inline std::vector<double> compute_cluster_texel_demands(
    const Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const TextureSizingOptions &options) {
    std::vector<double> demands(merged.cluster_count(), 0.0);

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        const double source_density = compute_source_texel_density(source, partition_to_clusters.segment(index));
        demands[index] = compute_target_texel_density(options, source_density) * cluster.triangle_count();
    }

    return demands;
}

// The clusters that can be textured at all, being the ones whose sources carry one.
[[nodiscard]]
inline std::vector<uint32_t> find_clusters_to_texture(
    const Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters) {
    std::vector<uint32_t> to_texture;

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        if (any_source_texture(source, partition_to_clusters.segment(index))) {
            to_texture.push_back(index);
        }
    }

    return to_texture;
}

// Plan the bake of one texture, sized for the clusters that will share it.
[[nodiscard]]
inline BakePlan plan_bake(
    const Clustering &merged,
    const std::span<const double> demands,
    std::vector<uint32_t> clusters,
    const double aspect) {
    const TextureDemand demand{
        .texels = sum(clusters, [&](const uint32_t index) {
            return demands[index];
        }),
        .utilization = sum(clusters, [&](const uint32_t index) {
            return compute_utilization(merged.clusters[index]);
        })
    };
    return BakePlan{
        .clusters = std::move(clusters),
        .size = compute_target_size(demand, aspect)
    };
}

// Chart each cluster on its own, so every chart boundary is also a cluster boundary.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters_separately(
    Clustering &merged,
    const std::span<const uint32_t> to_texture,
    const std::span<const double> demands,
    const uv::AtlasOptions &options) {
    std::vector<BakePlan> plans;

    for (const uint32_t index : to_texture) {
        const Cluster &cluster = merged.clusters[index];

        // Perform unwrap at approximately the final resolution
        const std::vector<glm::dvec3> positions = merged.get_cluster_positions(index);
        uv::Atlas atlas = uv::build_atlas(cluster.local_triangles, positions, compute_unwrap_resolution(demands[index]), options);
        if (atlas.uvs.empty()) {
            LOG_WARN("Cluster {} could not be unwrapped, leaving it untextured", index);
            continue;
        }

        const double aspect = atlas.aspect();

        // Apply new uvs and duplicated seam vertices
        merged.clusters[index] = apply_atlas(cluster, std::move(atlas));

        plans.push_back(plan_bake(merged, demands, {index}, aspect));
    }

    return plans;
}

// Chart every cluster as one surface, so charts may span cluster boundaries.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters_together(
    Clustering &merged,
    const std::span<const uint32_t> to_texture,
    const std::span<const double> demands,
    const uv::AtlasOptions &options) {
    std::vector<glm::uvec3> triangles;
    for (const uint32_t index : to_texture) {
        const Cluster &cluster = merged.clusters[index];
        for (const glm::uvec3 &local : cluster.local_triangles) {
            triangles.push_back(cluster.global_triangle(local));
        }
    }

    // Perform unwrap at approximately the final resolution
    const double demanded_texels = sum(to_texture, [&](const uint32_t index) {
        return demands[index];
    });
    const uv::Atlas atlas = uv::build_atlas(triangles, merged.positions, compute_unwrap_resolution(demanded_texels), options);
    if (atlas.uvs.empty()) {
        LOG_WARN("Node of {} clusters could not be unwrapped, leaving it untextured", to_texture.size());
        return {};
    }

    // Apply new uvs and duplicated seam vertices
    apply_node_atlas(merged, to_texture, atlas);

    std::vector<uint32_t> clusters(to_texture.begin(), to_texture.end());

    std::vector<BakePlan> plans;
    plans.push_back(plan_bake(merged, demands, std::move(clusters), atlas.aspect()));
    return plans;
}

// Give every cluster uvs to address its texture with.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters(
    Clustering &merged,
    const std::span<const uint32_t> to_texture,
    const std::span<const double> demands,
    const TextureOptions &options) {
    if (to_texture.empty()) {
        return {};
    }

    switch (options.charting) {
    case ChartingMode::PerCluster:
        return unwrap_clusters_separately(merged, to_texture, demands, options.atlas);
    case ChartingMode::PerNode:
        return unwrap_clusters_together(merged, to_texture, demands, options.atlas);
    default:
        UNREACHABLE();
    }
}

// Run every planned bake, at the size the node budget left it.
inline void bake_node_textures(
    Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const std::span<const BakePlan> plans,
    const BakeTextureOptions &options) {
    for (const BakePlan &plan : plans) {
        std::vector<uint32_t> source_clusters;
        for (const uint32_t index : plan.clusters) {
            const std::span<const uint32_t> segment = partition_to_clusters.segment(index);
            source_clusters.insert(source_clusters.end(), segment.begin(), segment.end());
        }

        const BakeSource bake_source = collect_bake_source(source, source_clusters);
        const cv::Mat baked = bake_clusters_texture(merged, plan.clusters, bake_source, plan.size, options);
        const uint32_t texture_id = merged.textures.add(baked);

        for (const uint32_t index : plan.clusters) {
            merged.clusters[index].texture_id = texture_id;
        }
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

    const std::vector<double> demands = compute_cluster_texel_demands(merged, source, partition_to_clusters, options.sizing);
    const std::vector<uint32_t> to_texture = find_clusters_to_texture(merged, source, partition_to_clusters);

    // No texture is baked until every cluster in the node has asked for a size
    std::vector<BakePlan> plans = unwrap_clusters(merged, to_texture, demands, options);
    rescale_to_fit_budget(plans, options.sizing.max_node_texels);

    // Render at new size
    bake_node_textures(merged, source, partition_to_clusters, plans, options.bake);

    return merged;
}
