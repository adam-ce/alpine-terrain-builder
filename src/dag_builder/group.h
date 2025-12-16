#pragma once

#include <vector>
#include <algorithm>

#include "cluster.h"
#include "meshopt.h"
#include "mesh/utils.h"

struct GroupOptions {
    uint32_t clusters_per_group = 4;
};

Clustering group(const Clustering &clustering, const GroupOptions options = {}) {
    const uint32_t clusters_per_group = options.clusters_per_group;
    ASSERT(clusters_per_group > 0, "Clusters per group must be greater than zero.");
    if (clusters_per_group == 1) {
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

    // Partition clusters into groups using the helper
    meshopt::PartitionClustersResult partition_result = meshopt::partition_clusters(
        cluster_vertices,
        cluster_vertex_counts,
        positions_f,
        clusters_per_group);
    const size_t group_count = partition_result.group_count;
    const auto &cluster_groups = partition_result.cluster_groups;

    // Build grouped clusters
    const uint32_t no_vertex_remap = -1;
    std::vector<uint32_t> vertex_remap(clustering.positions.size(), no_vertex_remap);

    std::vector<Cluster> grouped_clusters(group_count);
    for (uint32_t group_index = 0; group_index < group_count; group_index++) {
        Cluster &group = grouped_clusters[group_index];

        for (uint32_t i = 0; i < cluster_count; i++) {
            // Skip all clusters not in this group
            if (cluster_groups[i] != group_index) {
                continue;
            }

            const Cluster &cluster = clustering.clusters[i];

            // Add cluster vertices, avoiding duplicates
            for (uint32_t vertex_index : cluster.vertex_indices) {
                uint32_t &vertex_index_in_group = vertex_remap[vertex_index];
                if (vertex_index_in_group == no_vertex_remap) {
                    vertex_index_in_group = group.vertex_indices.size();
                    group.vertex_indices.push_back(vertex_index);
                }
            }

            // Remap triangles to new vertex indices
            for (const auto &triangle : cluster.local_triangles) {
                glm::uvec3 remapped;
                for (uint8_t k = 0; k < 3; k++) {
                    remapped[k] = vertex_remap[cluster.vertex_indices[triangle[k]]];
                }
                if (!is_degenerate(remapped)) {
                    group.local_triangles.push_back(remapped);
                }
            }
        }

        // Reset vertex remap for next group
        for (const uint32_t vertex_index : group.vertex_indices) {
            vertex_remap[vertex_index] = no_vertex_remap;
        }
    }

    return Clustering{
        clustering.positions,
        std::move(grouped_clusters)};
}
