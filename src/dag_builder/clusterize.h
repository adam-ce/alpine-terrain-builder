#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "meshopt.h"
#include "utils.h"
#include "validate.h"
#include "range_utils.h"

struct ClusterOptions {
    static constexpr uint32_t MAX_VERTEX_LIMIT = UINT8_MAX;
    static constexpr uint32_t MAX_TRIANGLE_LIMIT = UINT32_MAX;

    uint32_t max_vertices = MAX_VERTEX_LIMIT;
    uint32_t min_triangles = MAX_TRIANGLES_PER_CLUSTER;
    uint32_t max_triangles = MAX_TRIANGLES_PER_CLUSTER;
    float cone_weight = 0.5;
    float split_factor = 2.0;
};

inline std::vector<Cluster> clusterize(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const std::span<const glm::dvec2> uvs,
    const ClusterOptions &options = {},
    const std::span<const uint32_t> global_vertex_map = {}) {
    std::vector<Cluster> clusters;

    meshopt::BuildMeshletsResult meshlet_result = meshopt::build_meshlets(
        triangles,
        positions,
        options.max_vertices,
        options.min_triangles,
        options.max_triangles,
        options.cone_weight,
        options.split_factor);

    for (const auto &meshlet : meshlet_result.meshlets) {
        Cluster cluster;
        cluster.texture_id = 0;
        cluster.id = clusters.size();

        // Map meshlet vertices to global vertex indices
        if (global_vertex_map.empty()) {
            cluster.vertex_indices.insert(
                cluster.vertex_indices.end(),
                meshlet_result.vertex_indices.begin() + meshlet.vertex_offset,
                meshlet_result.vertex_indices.begin() + meshlet.vertex_offset + meshlet.vertex_count);
        } else {
            DEBUG_ASSERT(global_vertex_map.size() == positions.size());
            cluster.vertex_indices.reserve(meshlet.vertex_count);
            for (size_t i = 0; i < meshlet.vertex_count; i++) {
                const size_t original_index = meshlet_result.vertex_indices[meshlet.vertex_offset + i];
                cluster.vertex_indices.push_back(global_vertex_map[original_index]);
            }
        }

        // Copy triangles
        const size_t triangle_begin = meshlet.triangle_offset;
        const size_t triangle_end = triangle_begin + meshlet.triangle_count;
        cluster.local_triangles.insert(
            cluster.local_triangles.end(),
            meshlet_result.local_triangles.begin() + triangle_begin,
            meshlet_result.local_triangles.begin() + triangle_end);

        // Copy UVs
        if (!uvs.empty()) {
            DEBUG_ASSERT(uvs.size() == positions.size());
            cluster.uvs.resize(cluster.vertex_count());

            for (size_t i = 0; i < meshlet.vertex_count; i++) {
                const uint32_t local_index = meshlet_result.vertex_indices[meshlet.vertex_offset + i];
                cluster.uvs[i] = uvs[local_index];
            }
        }

        // We made no error here
        cluster.absolute_error = 0.0;

        clusters.push_back(std::move(cluster));
    }

    return clusters;
}

inline Clustering clusterize(mesh::Simple3d mesh, const ClusterOptions &options = {}) {
    const auto positions_f = to_approximate_normalized(mesh.positions);

    std::vector<Cluster> clusters = clusterize(
        mesh.triangles,
        positions_f,
        mesh.uvs,
        options,
        {}
    );

    TextureSet textures;
    if (mesh.texture.has_value()) {
        textures.add(mesh.texture.value());
    } else {
        textures.add(cv::Mat::zeros(1, 1, CV_8UC3));
    }

    return Clustering{
        std::move(mesh.positions),
        std::move(clusters),
        std::move(textures)};
}

inline std::vector<Cluster> clusterize(
    const Cluster &cluster,
    const std::span<const glm::dvec3> positions,
    const ClusterOptions &options = {}) {
    const std::vector<glm::dvec3> positions_d = collect_cluster_positions(cluster, positions);
    const std::vector<glm::vec3> positions_f = to_approximate_normalized(positions_d);

    return clusterize(
        cluster.local_triangles,
        positions_f,
        cluster.uvs,
        options,
        cluster.vertex_indices);
}

struct ClusteringAndBackwardMapping {
    Clustering clustering;
    std::vector<uint32_t> backward_mapping; // new cluster index -> original cluster index
};

inline ClusteringAndBackwardMapping clusterize(const Clustering &input, const ClusterOptions &options = {}) {
    std::vector<Cluster> new_clusters;

    const auto counts = input.clusters | std::views::transform(&Cluster::vertex_count);
    const size_t combined_vertex_count = sum(counts);
    const size_t max_cluster_vertex_count = input.clusters.empty() ? 0 : std::ranges::max(counts);

    // Preallocate position buffers
    std::vector<glm::dvec3> positions_d;
    std::vector<glm::vec3> positions_f;
    positions_d.reserve(max_cluster_vertex_count);
    positions_f.reserve(max_cluster_vertex_count);

    // Track parent cluster for each new cluster
    std::vector<uint32_t> parent_cluster_indices;
    const size_t expected_new_cluster_count = combined_vertex_count / options.max_vertices;
    parent_cluster_indices.reserve(expected_new_cluster_count);

    for (uint32_t cluster_index = 0; cluster_index < input.clusters.size(); cluster_index++) {
        const Cluster &cluster = input.clusters[cluster_index];

        // Materialize cluster positions as floats
        collect_cluster_positions(cluster, input.positions, positions_d);
        to_approximate_normalized(positions_d, positions_f);

        // Perform clustering on current cluster
        std::vector<Cluster> sub_clusters = clusterize(
            cluster.local_triangles,
            positions_f,
            cluster.uvs,
            options,
            cluster.vertex_indices);

        // Copy error to sub-clusters
        for (Cluster &sub_cluster : sub_clusters) {
            sub_cluster.absolute_error = cluster.absolute_error;
            sub_cluster.texture_id = cluster.texture_id;
        }

        // Record new clusters and their parent
        parent_cluster_indices.insert(parent_cluster_indices.end(), sub_clusters.size(), cluster_index);
        new_clusters.insert(new_clusters.end(),
                            std::make_move_iterator(sub_clusters.begin()),
                            std::make_move_iterator(sub_clusters.end()));
    }

    Clustering new_clustering{
        input.positions,
        std::move(new_clusters),
        input.textures
    };
    validate(new_clustering);
    return ClusteringAndBackwardMapping{std::move(new_clustering), std::move(parent_cluster_indices)};
}
