#pragma once

#include <vector>

#include "cluster.h"
#include "enumerate.h"
#include "mesh/VertexMap.h"

struct ClusteringAndMap {
    Clustering clustering;
    VertexMap remap;
};

inline bool selects_all_clusters_in_order(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    if (cluster_indices.size() != clustering.cluster_count()) {
        return false;
    }
    for (const auto &[index, cluster_index] : enumerate(cluster_indices)) {
        if (cluster_index != index) {
            return false;
        }
    }
    return true;
}

inline ClusteringAndMap slice_clusters_with_map(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t cluster_count = clustering.cluster_count();
    const uint32_t vertex_count = clustering.vertex_count();
    if (selects_all_clusters_in_order(clustering, cluster_indices)) {
        return {clustering, VertexMap::identity(vertex_count)};
    }

    Clustering new_clustering;
    const uint32_t approximate_vertex_count = std::min(vertex_count, static_cast<uint32_t>(vertex_count * static_cast<float>(cluster_indices.size()) / static_cast<float>(cluster_count) * 1.5));
    new_clustering.positions.reserve(approximate_vertex_count);

    constexpr uint32_t invalid_remap = VertexMap::invalid_index;
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
            new_texture_id = new_clustering.textures.add(texture);
        }

        Cluster new_cluster{
            .id = cluster.id,
            .vertex_indices = new_vertex_indices,
            .local_triangles = cluster.local_triangles,
            .uvs = cluster.uvs,
            .texture_id = new_texture_id,
            .absolute_error = cluster.absolute_error};
        new_clustering.clusters.push_back(new_cluster);
    }

    return {std::move(new_clustering), VertexMap::from_forward(std::move(vertex_remap))};
}

inline Clustering slice_clusters(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    return slice_clusters_with_map(clustering, cluster_indices).clustering;
}
