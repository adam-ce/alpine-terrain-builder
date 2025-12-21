#pragma once

#include <vector>
#include <algorithm>

#include "cluster.h"
#include "meshopt.h"
#include "mesh/utils.h"

struct PartitionOptions {
    uint32_t clusters_per_partition = 4;
};

Clustering partition(const Clustering &clustering, const PartitionOptions options = {}) {
    const uint32_t clusters_per_partition = options.clusters_per_partition;
    ASSERT(clusters_per_partition > 0, "Clusters per partition must be greater than zero.");
    if (clusters_per_partition == 1) {
        return clustering;
    }

    // meshopt only supports float positions
    const std::vector<glm::vec3> positions_f = to_approximate_normalized(clustering.positions);

    // Prepare vertex counts per cluster and total vertex count
    const uint32_t cluster_count = clustering.clusters.size();
    std::vector<uint32_t> cluster_vertex_counts(cluster_count);
    uint32_t total_index_count = 0;
    for (uint32_t i = 0; i < cluster_count; i++) {
        cluster_vertex_counts[i] = clustering.clusters[i].vertex_indices.size();
        total_index_count += cluster_vertex_counts[i];
    }

    // Flatten vertices into index buffer
    std::vector<uint32_t> cluster_vertices(total_index_count);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < cluster_count; i++) {
        const auto &vertices = clustering.clusters[i].vertex_indices;
        std::copy(vertices.begin(), vertices.end(), cluster_vertices.begin() + offset);
        offset += vertices.size();
    }

    // Partition clusters into partitions using the helper
    meshopt::PartitionClustersResult partition_result = meshopt::partition_clusters(
        cluster_vertices,
        cluster_vertex_counts,
        positions_f,
        clusters_per_partition);
    const size_t partition_count = partition_result.partition_count;
    const auto &cluster_partitions = partition_result.cluster_partitions;

    // Build partitioned clusters
    const uint32_t no_vertex_remap = -1;
    std::vector<uint32_t> vertex_remap(clustering.positions.size(), no_vertex_remap);

    std::vector<Cluster> partitioned_clusters(partition_count);
    for (uint32_t partition_index = 0; partition_index < partition_count; partition_index++) {
        Cluster &partition = partitioned_clusters[partition_index];

        for (uint32_t i = 0; i < cluster_count; i++) {
            // Skip all clusters not in this partition
            if (cluster_partitions[i] != partition_index) {
                continue;
            }

            const Cluster &cluster = clustering.clusters[i];

            // Add cluster vertices, avoiding duplicates
            for (uint32_t vertex_index : cluster.vertex_indices) {
                uint32_t &vertex_index_in_partition = vertex_remap[vertex_index];
                if (vertex_index_in_partition == no_vertex_remap) {
                    vertex_index_in_partition = partition.vertex_indices.size();
                    partition.vertex_indices.push_back(vertex_index);
                }
            }

            // Remap triangles to new vertex indices
            for (const auto &triangle : cluster.local_triangles) {
                glm::uvec3 remapped;
                for (uint8_t k = 0; k < 3; k++) {
                    remapped[k] = vertex_remap[cluster.vertex_indices[triangle[k]]];
                }
                if (!is_degenerate(remapped)) {
                    partition.local_triangles.push_back(remapped);
                }
            }
        }

        // Reset vertex remap for next partition
        for (const uint32_t vertex_index : partition.vertex_indices) {
            vertex_remap[vertex_index] = no_vertex_remap;
        }
    }

    return Clustering{
        clustering.positions,
        std::move(partitioned_clusters)};
}
