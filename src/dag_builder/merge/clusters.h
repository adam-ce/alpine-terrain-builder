#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <ranges>

#include <glm/glm.hpp>

#include "build_config.h"
#include "cluster.h"
#include "enumerate.h"
#include "log.h"
#include "mesh/connectivity/adjacency.h"
#include "range_utils.h"
#include "validate.h"
#include "Partitioning.h"


namespace detail {
constexpr uint32_t no_vertex_remap = -1;

// The scratch buffer comes in fully reset, and every merge has to leave it that way again.
inline void check_clean_vertex_remap(const std::span<uint32_t> vertex_remap, const uint32_t vertex_count) {
    if constexpr (IS_DEBUG_BUILD) {
        DEBUG_ASSERT(vertex_remap.size() == vertex_count);
        for (const uint32_t vertex_index : vertex_remap) {
            ALP_UNUSED(vertex_index);
            DEBUG_ASSERT(vertex_index == no_vertex_remap);
        }
    } else {
        ALP_UNUSED(vertex_remap, vertex_count);
    }
}

// Build one partition's cluster: concatenate the triangles, deduplicate the vertices, carry the error.
inline Cluster build_partition_cluster(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const std::span<uint32_t> vertex_remap /* scratch buffer */
) {
    check_clean_vertex_remap(vertex_remap, clustering.vertex_count());

    const uint32_t cluster_count = cluster_indices.size();

    Cluster merged;
    for (uint32_t linear_cluster_index = 0; linear_cluster_index < cluster_count; linear_cluster_index++) {
        const uint32_t cluster_index = cluster_indices[linear_cluster_index];
        const Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t vertex_count = cluster.vertex_count();

        // Create vertex remapping from original vertex indices to merged vertex indices
        for (uint32_t local_vertex_index = 0; local_vertex_index < vertex_count; local_vertex_index++) {
            const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
            uint32_t &merged_vertex_index = vertex_remap[global_vertex_index];

            if (merged_vertex_index == no_vertex_remap) {
                // This vertex was not yet remapped -> assign new merged index.
                merged_vertex_index = merged.vertex_indices.size();
                merged.vertex_indices.push_back(global_vertex_index);
            }
        }

        // Remap triangles to new vertex indices
        for (const auto &triangle : cluster.local_triangles) {
            glm::uvec3 remapped;
            for (uint8_t k = 0; k < 3; k++) {
                remapped[k] = vertex_remap[cluster.vertex_indices[triangle[k]]];
                DEBUG_ASSERT(remapped[k] != no_vertex_remap);
            }
            if (!mesh::is_degenerate(remapped)) {
                merged.local_triangles.push_back(remapped);
            }
        }
    }

    // Reset vertex remap
    for (const uint32_t vertex_index : merged.vertex_indices) {
        vertex_remap[vertex_index] = no_vertex_remap;
    }

    // Carry the largest child error into the merged cluster.
    merged.absolute_error = max(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].absolute_error;
    });

    return merged;
}

}

// Concatenate the clusters of each partition into one, carrying geometry only.
[[nodiscard]]
inline Clustering merge_clusters(const Clustering &clustering, const Partitioning &partitioning) {
    const PartitionToClusters partition_to_clusters = invert_partitioning(partitioning);

    // Shared buffer across all the partitions.
    std::vector<uint32_t> vertex_remap(clustering.vertex_count(), detail::no_vertex_remap);

    std::vector<Cluster> merged_clusters = transform_vector(partition_to_clusters.segments(), [&](const std::span<const uint32_t> cluster_indices) {
        ASSERT(!cluster_indices.empty());
        return detail::build_partition_cluster(clustering, cluster_indices, vertex_remap);
    });

    Clustering merged{.positions = clustering.positions, .clusters = std::move(merged_clusters)};
    LOG_DEBUG("Merged {} clusters into {} partitions", clustering.cluster_count(), partitioning.partition_count);
    validate(merged);
    return merged;
}
