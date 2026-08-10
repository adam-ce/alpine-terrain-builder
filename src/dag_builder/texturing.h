#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "atlas/BakeSource.h"
#include "atlas/bake_cluster_texture.h"
#include "cluster.h"
#include "compact.h"
#include "enumerate.h"
#include "log.h"
#include "Partitioning.h"
#include "range_utils.h"
#include "texture_reuse.h"
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
        return clustering.clusters[i].is_textured();
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
    bool allow_texture_reuse = true;
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

// The clusters needing an unwrap: those whose sources carry a texture, but which the merge
// could not carry one into.
[[nodiscard]]
inline std::vector<uint32_t> find_clusters_to_unwrap(
    const Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters) {
    std::vector<uint32_t> to_unwrap;

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        if (cluster.is_textured() || !any_source_texture(source, partition_to_clusters.segment(index))) {
            continue;
        }
        to_unwrap.push_back(index);
    }

    return to_unwrap;
}

// Width over height of a texture.
[[nodiscard]]
inline double compute_aspect(const glm::uvec2 size) {
    return double(size.x) / size.y;
}

// Size one texture for the clusters that will share it.
[[nodiscard]]
inline BakePlan size_texture(
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

// Collect the clusters that kept a carried-in texture, one entry per texture.
[[nodiscard]]
inline std::vector<BakePlan> size_inherited_textures(
    const Clustering &merged,
    const std::span<const double> demands) {
    constexpr uint32_t no_entry = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> entry_of_texture(merged.textures.size(), no_entry);
    std::vector<std::vector<uint32_t>> clusters_per_texture;
    std::vector<uint32_t> texture_ids;

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        if (!cluster.is_textured()) {
            continue;
        }
        const uint32_t texture_id = cluster.texture_id.value();
        uint32_t &entry = entry_of_texture[texture_id];
        if (entry == no_entry) {
            entry = clusters_per_texture.size();
            clusters_per_texture.emplace_back();
            texture_ids.push_back(texture_id);
        }
        clusters_per_texture[entry].push_back(index);
    }

    std::vector<BakePlan> plans;
    plans.reserve(texture_ids.size());
    for (const auto [entry, texture_id] : enumerate(texture_ids)) {
        const glm::uvec2 current_size = get_texture_size(merged.textures[texture_id]);
        BakePlan plan = size_texture(merged, demands, std::move(clusters_per_texture[entry]), compute_aspect(current_size));
        plan.inherited_id = texture_id;
        // Upscaling adds no detail, so never plan more than the texture already holds.
        plan.size = glm::min(plan.size, current_size);
        plans.push_back(std::move(plan));
    }

    return plans;
}

// Chart each cluster on its own, so every chart boundary is also a cluster boundary.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters_separately(
    Clustering &merged,
    const std::span<const uint32_t> to_unwrap,
    const std::span<const double> demands,
    const uv::AtlasOptions &options) {
    std::vector<BakePlan> plans;

    for (const uint32_t index : to_unwrap) {
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

        plans.push_back(size_texture(merged, demands, {index}, aspect));
    }

    return plans;
}

// Chart every cluster as one surface, so charts may span cluster boundaries.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters_together(
    Clustering &merged,
    const std::span<const uint32_t> to_unwrap,
    const std::span<const double> demands,
    const uv::AtlasOptions &options) {
    std::vector<glm::uvec3> triangles;
    for (const uint32_t index : to_unwrap) {
        const Cluster &cluster = merged.clusters[index];
        for (const glm::uvec3 &local : cluster.local_triangles) {
            triangles.push_back(cluster.global_triangle(local));
        }
    }

    // Perform unwrap at approximately the final resolution
    const double demanded_texels = sum(to_unwrap, [&](const uint32_t index) {
        return demands[index];
    });
    const uv::Atlas atlas = uv::build_atlas(triangles, merged.positions, compute_unwrap_resolution(demanded_texels), options);
    if (atlas.uvs.empty()) {
        LOG_WARN("Node of {} clusters could not be unwrapped, leaving it untextured", to_unwrap.size());
        return {};
    }

    // Apply new uvs and duplicated seam vertices
    apply_node_atlas(merged, to_unwrap, atlas);

    std::vector<uint32_t> clusters(to_unwrap.begin(), to_unwrap.end());

    std::vector<BakePlan> plans;
    plans.push_back(size_texture(merged, demands, std::move(clusters), atlas.aspect()));
    return plans;
}

// Give every cluster uvs to address its texture with.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters(
    Clustering &merged,
    const std::span<const uint32_t> to_unwrap,
    const std::span<const double> demands,
    const TextureOptions &options) {
    if (to_unwrap.empty()) {
        return {};
    }

    switch (options.charting) {
    case ChartingMode::PerCluster:
        return unwrap_clusters_separately(merged, to_unwrap, demands, options.atlas);
    case ChartingMode::PerNode:
        return unwrap_clusters_together(merged, to_unwrap, demands, options.atlas);
    default:
        UNREACHABLE();
    }
}

// Resize every carried-in texture to what its clusters were budgeted.
inline void rescale_inherited_textures(Clustering &merged, const std::span<const BakePlan> textures) {
    for (const BakePlan &plan : textures) {
        if (!plan.inherited_id.has_value()) {
            continue;
        }
        cv::Mat &texture = merged.textures[plan.inherited_id.value()];
        if (get_texture_size(texture) != plan.size) {
            texture = rescale_texture(texture, plan.size);
        }
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
        if (plan.inherited_id.has_value()) {
            continue;
        }

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

    // Carry over the source texture wherever the merged clusters can keep addressing it
    if (options.allow_texture_reuse) {
        inherit_shared_textures(merged, source, partition_to_clusters);
    }

    const std::vector<double> demands = compute_cluster_texel_demands(merged, source, partition_to_clusters, options.sizing);
    const std::vector<uint32_t> to_unwrap = find_clusters_to_unwrap(merged, source, partition_to_clusters);

    // No texture is resized or baked until every cluster in the node has asked for a size
    std::vector<BakePlan> textures = size_inherited_textures(merged, demands);
    for (BakePlan &unwrapped : unwrap_clusters(merged, to_unwrap, demands, options)) {
        textures.push_back(std::move(unwrapped));
    }
    rescale_to_fit_budget(textures, options.sizing.max_node_texels);

    // Realize at new size
    rescale_inherited_textures(merged, textures);
    bake_node_textures(merged, source, partition_to_clusters, textures, options.bake);

    remove_unused_textures_inplace(merged);
    return merged;
}
