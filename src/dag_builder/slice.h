#pragma once

#include <vector>

#include "cluster.h"

inline Clustering slice_clusters(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t cluster_count = clustering.cluster_count();
    const uint32_t vertex_count = clustering.vertex_count();
    if (cluster_indices.size() == clustering.cluster_count()) {
        return clustering;
    }

    Clustering new_clustering;
    const uint32_t approximate_vertex_count = std::min(vertex_count, static_cast<uint32_t>(vertex_count * static_cast<float>(cluster_indices.size()) / static_cast<float>(cluster_count) * 1.5));
    new_clustering.positions.reserve(approximate_vertex_count);

    constexpr uint32_t invalid_remap = -1;
    std::vector<uint32_t> vertex_remap(clustering.vertex_count(), invalid_remap);
    std::vector<uint32_t> texture_remap(clustering.textures.size(), invalid_remap);

    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];

        std::vector<uint32_t> new_vertex_indices;
        new_vertex_indices.reserve(cluster.vertex_count());
        for (const uint32_t vertex_index : cluster.vertex_indices) {
            uint32_t& new_vertex_index = vertex_remap[vertex_index];
            if (new_vertex_index == invalid_remap) {
                const glm::dvec3 &position = clustering.positions[vertex_index];
                new_vertex_index = new_clustering.positions.size();
                new_clustering.positions.push_back(position);
            }
            new_vertex_indices.push_back(new_vertex_index);
        }

        uint32_t &new_texture_id = texture_remap[cluster.texture_id];
        if (new_texture_id == invalid_remap) {
            const cv::Mat &texture = clustering.textures[cluster.texture_id];
            new_texture_id = new_clustering.textures.size();
            new_clustering.textures.push_back(texture);
        }

        Cluster new_cluster{
            .vertex_indices = new_vertex_indices,
            .local_triangles = cluster.local_triangles,
            .uvs = cluster.uvs,
            .texture_id = cluster.texture_id,
            .absolute_error = cluster.absolute_error};
        new_clustering.clusters.push_back(new_cluster);
    }

    return new_clustering;
}
