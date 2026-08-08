#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

#include "cluster.h"
#include "range_utils.h"

struct UvRef {
    uint32_t map_index = 0;
    glm::uvec3 uvs; // indices into uv_maps[map_index]
};

// The surface before simplification, in the uv spaces the bake reads back through.
struct BakeSource {
    std::span<const glm::dvec3> positions;
    std::vector<glm::uvec3> triangles; // indices into positions
    std::vector<UvRef> uv_triangles; // per triangle
    std::vector<std::vector<glm::dvec2>> uv_maps; // per contributing cluster
    std::vector<cv::Mat> images; // parallel to uv_maps
};

// Gather the given clusters into one surface, in the uv spaces a bake samples through.
[[nodiscard]]
inline BakeSource collect_bake_source(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t triangle_count = sum(cluster_indices, [&](const uint32_t index) {
        return clustering.clusters[index].triangle_count();
    });

    BakeSource source;
    source.positions = clustering.positions;
    source.uv_maps.reserve(cluster_indices.size());
    source.images.reserve(cluster_indices.size());
    source.triangles.reserve(triangle_count);
    source.uv_triangles.reserve(triangle_count);

    // Clusters with no texture or uvs get a placeholder black.
    const cv::Mat placeholder_image = cv::Mat::zeros(1, 1, CV_8UC3);

    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        const bool is_textured = cluster.has_texture() && cluster.has_uvs();

        const uint32_t map_index = source.uv_maps.size();
        if (is_textured) {
            source.uv_maps.push_back(cluster.uvs);
            source.images.push_back(clustering.textures[cluster.texture_id.value()]);
        } else {
            source.uv_maps.emplace_back(cluster.vertex_count(), glm::dvec2(0));
            source.images.push_back(placeholder_image);
        }

        for (const glm::uvec3 &local : cluster.local_triangles) {
            source.triangles.push_back(cluster.global_triangle(local));
            source.uv_triangles.push_back(UvRef{.map_index = map_index, .uvs = local});
        }
    }

    return source;
}