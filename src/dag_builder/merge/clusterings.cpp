#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "cluster.h"
#include "validate.h"
#include "enumerate.h"
#include "compact.h"
#include "merge/clusterings.h"
#include "mesh/triangle_compare.h"
#include "spatial_lookup/Hashmap.h"

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

Clustering merge_clusterings(const std::span<const Clustering> clusterings, const double epsilon) {
    if (clusterings.empty()) {
        return {};
    }
    if (clusterings.size() == 1) {
        return clusterings[0];
    }

    // Compute per-clustering base offsets for vertex_to_canonical lookup
    const uint32_t total_vertex_count = sum(clusterings, [&](const auto &c) { return c.vertex_count(); });
    std::vector<uint32_t> base_offsets(clusterings.size());
    {
        uint32_t offset = 0;
        for (size_t ci = 0; ci < clusterings.size(); ci++) {
            base_offsets[ci] = offset;
            offset += clusterings[ci].vertex_count();
        }
    }

    // Build a position -> canonical index mapping using epsilon-ball deduplication
    std::vector<uint32_t> vertex_to_canonical(total_vertex_count);
    spatial_lookup::Hashmap3d<uint32_t> vertex_lookup(epsilon);
    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(total_vertex_count);

    std::vector<uint32_t> matches;
    for (size_t ci = 0; ci < clusterings.size(); ci++) {
        for (uint32_t vi = 0; vi < clusterings[ci].vertex_count(); vi++) {
            const glm::dvec3 &position = clusterings[ci].positions[vi];
            const uint32_t global_index = base_offsets[ci] + vi;
            matches.clear();
            if (vertex_lookup.find_all_near(position, epsilon, matches)) {
                vertex_to_canonical[global_index] = matches[0];
            } else {
                const uint32_t next_index = static_cast<uint32_t>(new_positions.size());
                vertex_lookup.insert(position, next_index);
                new_positions.push_back(position);
                vertex_to_canonical[global_index] = next_index;
            }
        }
    }

    const size_t unique_count = new_positions.size();
    const size_t shared_count = total_vertex_count - unique_count;
    LOG_DEBUG("Merging with {} shared and {} unique vertices", shared_count, unique_count);

    Clustering merged;
    merged.positions = std::move(new_positions);

    for (size_t ci = 0; ci < clusterings.size(); ci++) {
        const Clustering &clustering = clusterings[ci];
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
                new_cluster.vertex_indices.push_back(vertex_to_canonical[base_offsets[ci] + vertex_index]);
            }

            merged.clusters.push_back(std::move(new_cluster));
        }
    }

    // Remove degenerate triangles
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
