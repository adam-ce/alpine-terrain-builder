#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "Partitioning.h"
#include "cluster.h"
#include "enumerate.h"
#include "log.h"
#include "range_utils.h"

namespace detail {

struct VertexUv {
    uint32_t global_index;
    glm::dvec2 uv;
};

// Collect list of uvs per global vertex as an ordered list for duplication checking.
[[nodiscard]]
inline std::vector<VertexUv> collect_vertex_uvs(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    std::vector<VertexUv> entries;
    entries.reserve(sum(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].uvs.size();
    }));

    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        for (const auto [local, global] : enumerate(cluster.vertex_indices)) {
            entries.push_back({global, cluster.uvs[local]});
        }
    }

    std::ranges::sort(entries, {}, &VertexUv::global_index);
    return entries;
}

// Whether neighbouring clusters give shared vertices they share the same uv.
[[nodiscard]]
inline bool has_consistent_uvs(const std::span<const VertexUv> entries) {
    const auto disagrees = [](const VertexUv &a, const VertexUv &b) {
        return a.global_index == b.global_index && a.uv != b.uv;
    };
    return std::ranges::adjacent_find(entries, disagrees) == entries.end();
}

// The texture every cluster in the partition carries, if they agree on one.
[[nodiscard]]
inline std::optional<uint32_t> find_shared_texture_id(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const std::optional<uint32_t> texture_id = clustering.clusters[cluster_indices[0]].texture_id;
    const bool agree = std::ranges::all_of(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].texture_id == texture_id;
    });
    return agree ? texture_id : std::nullopt;
}

// The uv the sources gave a vertex.
[[nodiscard]]
inline glm::dvec2 find_vertex_uv(const std::span<const VertexUv> entries, const uint32_t global_index) {
    const auto it = std::ranges::lower_bound(entries, global_index, {}, &VertexUv::global_index);
    DEBUG_ASSERT(it != entries.end() && it->global_index == global_index);
    return it->uv;
}
} // namespace detail

// Give a merged cluster the texture its sources shared, if they did and had no uv seams between them.
// Returns whether the texture could be carried over.
inline bool inherit_shared_texture(
    Cluster &merged,
    TextureSet &textures,
    const Clustering &source,
    const std::span<const uint32_t> source_clusters) {
    const std::optional<uint32_t> texture_id = detail::find_shared_texture_id(source, source_clusters);
    if (!texture_id.has_value()) {
        return false;
    }

    const std::vector<detail::VertexUv> entries = detail::collect_vertex_uvs(source, source_clusters);
    if (!detail::has_consistent_uvs(entries)) {
        return false;
    }

    merged.texture_id = textures.add(source.textures[texture_id.value()]);
    merged.uvs = transform_vector(merged.vertex_indices, [&](const uint32_t global) {
        return detail::find_vertex_uv(entries, global);
    });
    return true;
}

// Carry the source textures into the merged clusters that can keep them unchanged.
inline void inherit_shared_textures(
    Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters) {
    DEBUG_ASSERT(merged.cluster_count() == partition_to_clusters.segment_count());

    for (const auto [cluster_index, cluster] : enumerate(merged.clusters)) {
        inherit_shared_texture(merged.clusters[cluster_index], merged.textures, source, partition_to_clusters.segment(cluster_index));
    }

    LOG_DEBUG("Carried a source texture into {} of {} merged clusters", std::ranges::count_if(merged.clusters, &Cluster::has_texture), merged.cluster_count());
}
