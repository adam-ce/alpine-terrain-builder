#include <vector>
#include <span>
#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "cluster.h"
#include "validate.h"
#include "quantize.h"
#include "enumerate.h"
#include "compact.h"
#include "merge.h"
#include "mesh/triangle_compare.h"

namespace detail {
/*
std::array<uint8_t, 16> hash_image(const cv::Mat &input_image) {
    std::array<uint8_t, 16> hash_buffer;
    cv::Mat hash_wrapper(1, 16, CV_8UC1, hash_buffer.data());
    cv::img_hash::averageHash(input_image, hash_wrapper);
    return hash_buffer;
}
*/
}

Clustering merge_clusterings(const std::span<const Clustering> clusterings, const double quantize_epsilon) {
    if (clusterings.size() == 1) {
        return clusterings[0];
    }

    using Quantized = glm::vec<3, int64_t>;

    // Create new position buffer
    const uint32_t vertex_count_bound = sum(clusterings, [&](const auto &clustering) { return clustering.vertex_count(); });
    std::unordered_map<Quantized, uint32_t> vertex_remap;
    vertex_remap.reserve(vertex_count_bound);
    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(vertex_count_bound);
    for (const Clustering &clustering : clusterings) {
        for (const glm::dvec3 &position : clustering.positions) {
            const Quantized quantized = quantize_index(position, quantize_epsilon);
            if (!vertex_remap.contains(quantized)) {
                // insert new mapping
                const uint32_t new_index = new_positions.size();
                vertex_remap.emplace(quantized, new_index);
                new_positions.push_back(position);
            }
        }
    }

    const size_t unique_count = new_positions.size();
    const size_t shared_count = vertex_count_bound - unique_count;
    LOG_DEBUG("Merging with {} shared and {} unique vertices", shared_count, unique_count);

    Clustering merged;
    merged.positions = new_positions;

    for (const Clustering &clustering : clusterings) {
        // Merge textures
        std::vector<uint32_t> new_texture_ids;
        new_texture_ids.reserve(clustering.textures.size());
        for (const cv::Mat &texture : clustering.textures) {
            new_texture_ids.push_back(merged.textures.add(texture));
        }

        // Merge clusters with adjusted indices
        for (const Cluster &cluster : clustering.clusters) {
            Cluster new_cluster;
            new_cluster.local_triangles = cluster.local_triangles;
            new_cluster.uvs = cluster.uvs;

            new_cluster.id = cluster.id;
            new_cluster.texture_id = new_texture_ids[cluster.texture_id];

            new_cluster.vertex_indices.reserve(cluster.vertex_count());
            for (const uint32_t vertex_index : cluster.vertex_indices) {
                const glm::dvec3 &position = clustering.positions[vertex_index];
                const Quantized quantized = quantize_index(position, quantize_epsilon);
                const uint32_t new_index = vertex_remap[quantized];
                new_cluster.vertex_indices.push_back(new_index);
            }

            merged.clusters.push_back(std::move(new_cluster));
        }
    }

    // Remove degenerate triangles
    std::vector<uint32_t> triangles_to_remove;
    for (Cluster &cluster : merged.clusters) {
        const size_t removed = std::erase_if(cluster.local_triangles, [&](const auto &local_triangle) {
            const glm::uvec3 global_triangle(
                cluster.vertex_indices[local_triangle.x],
                cluster.vertex_indices[local_triangle.y],
                cluster.vertex_indices[local_triangle.z]);
            return mesh::is_degenerate(global_triangle);
        });

        if (removed > 0) {
            compact_cluster_inplace(cluster);
        }
    }

    validate(merged);
    return merged;
}
