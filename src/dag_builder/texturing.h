#pragma once

#include <algorithm>
#include <ranges>
#include <span>
#include <unordered_map>
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
#include "utils.h"
#include "uv/atlas.h"

namespace detail {
inline bool check_all_same_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t cluster_count = cluster_indices.size();
    if (cluster_count <= 1) {
        return true;
    }

    const Cluster &first_cluster = clustering.clusters[cluster_indices[0]];
    for (uint32_t i = 1; i < cluster_count; i++) {
        const Cluster &cluster = clustering.clusters[cluster_indices[i]];
        if (cluster.has_uvs() != first_cluster.has_uvs()) {
            return false;
        }
        if (cluster.texture_id != first_cluster.texture_id) {
            return false;
        }
    }
    return true;
}

inline bool check_consistent_uvs(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t uv_count = sum(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].uvs.size();
    });

    struct VertexEntry {
        uint32_t global_index;
        glm::dvec2 uv;
    };

    // Buffer for vertex entries
    std::vector<VertexEntry> buffer;
    buffer.reserve(uv_count);

    // Collect all cluster vertices
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        if (!cluster.has_uvs()) {
            continue;
        }

        for (const auto [local, global] : enumerate(cluster.vertex_indices)) {
            buffer.push_back({global, cluster.uvs[local]});
        }
    }
    DEBUG_ASSERT(buffer.size() == uv_count);

    // Sort vertices by global index
    std::sort(buffer.begin(), buffer.end(), [](const VertexEntry &a, const VertexEntry &b) {
        return a.global_index < b.global_index;
    });

    // Check for inconsistent uvs.
    for (uint32_t i = 1; i < uv_count; i++) {
        if (buffer[i].global_index == buffer[i - 1].global_index) {
            if (buffer[i].uv != buffer[i - 1].uv) {
                return false;
            }
        }
    }

    return true;
}
} // namespace detail

// Whether any of the clusters carries a texture.
inline bool any_source_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    return std::ranges::any_of(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].has_texture();
    });
}

// Whether the clusters share one texture and agree on the uvs addressing it.
inline bool can_inherit_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    return detail::check_all_same_texture(clustering, cluster_indices)
        && detail::check_consistent_uvs(clustering, cluster_indices);
}

// Adopt the source clusters' shared texture and their uvs for the vertices that survived simplification.
inline void inherit_source_texture(
    Cluster &cluster,
    TextureSet &textures,
    const Clustering &source,
    const std::span<const uint32_t> cluster_indices) {
    // The sources agree on both, so the first one speaks for all of them.
    const Cluster &first = source.clusters[cluster_indices[0]];
    if (!first.has_texture() || !first.has_uvs()) {
        return;
    }

    std::unordered_map<uint32_t, glm::dvec2> uv_by_vertex;
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &source_cluster = source.clusters[cluster_index];
        for (const auto [local, global] : enumerate(source_cluster.vertex_indices)) {
            uv_by_vertex.emplace(global, source_cluster.uvs[local]);
        }
    }

    cluster.texture_id = textures.add(source.textures[first.texture_id.value()]);
    cluster.uvs = transform_vector(cluster.vertex_indices, [&](const uint32_t global) {
        return uv_by_vertex.at(global);
    });
}

struct TextureOptions {
    uv::AtlasOptions atlas = {};
    BakeTextureOptions bake = {};
    // Inherit a child texture when every source cluster shares it and their uvs agree.
    bool allow_texture_reuse = true;
};

// Unwrap and bake a fresh texture for every merged cluster that cannot inherit one.
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
        if (options.allow_texture_reuse && can_inherit_texture(source, cluster_indices)) {
            inherit_source_texture(cluster, merged.textures, source, cluster_indices);
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
        const BakeSource bake_source = collect_bake_source(source, cluster_indices);
        cluster.texture_id = merged.textures.add(
            bake_cluster_texture(cluster, merged.positions, bake_source, size, options.bake));
    }

    trim_textures_inplace(merged);
    return merged;
}