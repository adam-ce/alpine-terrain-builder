#pragma once

#include <vector>
#include <algorithm>

#include "cluster.h"
#include "meshopt.h"
#include "vector_utils.h"

struct PartitionOptions {
    uint32_t clusters_per_partition = 4;
};

struct ClusteringAndForwardMapping {
    Clustering clustering;
    std::vector<uint32_t> forward_mapping; // original cluster index -> new cluster index

    operator Clustering() const & {
        return clustering;
    }
    operator Clustering() && {
        return std::move(clustering);
    }
};

ClusteringAndForwardMapping partition(const Clustering &clustering, const PartitionOptions options = {}) {
    const uint32_t clusters_per_partition = options.clusters_per_partition;
    if (clusters_per_partition == 1) {
        std::vector<uint32_t> identity(clustering.cluster_count());
        std::iota(identity.begin(), identity.end(), 0);
        return ClusteringAndForwardMapping{clustering, identity};
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
    const std::vector<uint32_t> cluster_partitions = std::move(partition_result.cluster_partitions);

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
                    DEBUG_ASSERT(remapped[k] != no_vertex_remap);
                }
                if (!is_degenerate(remapped)) {
                    partition.local_triangles.push_back(remapped);
                }
            }

            // Copy UVs
            if (!cluster.uvs.empty()) {
                partition.uvs.resize(partition.vertex_count());
                for (size_t vertex_index = 0; vertex_index < cluster.vertex_count(); vertex_index++) {
                    const uint32_t original_index = cluster.vertex_indices[vertex_index];
                    const uint32_t new_index = vertex_remap[original_index];
                    partition.uvs[new_index] = cluster.uvs[vertex_index];
                }
            }
        }

        // Reset vertex remap for next partition
        for (const uint32_t vertex_index : partition.vertex_indices) {
            vertex_remap[vertex_index] = no_vertex_remap;
        }
    }

    Clustering new_clustering{
        clustering.positions,
        std::move(partitioned_clusters),
        clustering.texture
    };
    return ClusteringAndForwardMapping{std::move(new_clustering), std::move(cluster_partitions)};
}
