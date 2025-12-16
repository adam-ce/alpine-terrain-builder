#pragma once

#include <vector>

#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "meshopt.h"
#include "utils.h"

struct ClusterOptions {
    static constexpr uint32_t MAX_VERTEX_LIMIT = UINT8_MAX;
    static constexpr uint32_t MAX_TRIANGLE_LIMIT = UINT32_MAX;

    uint32_t max_vertices = MAX_VERTEX_LIMIT;
    uint32_t min_triangles = 256;
    uint32_t max_triangles = 256;
    float cone_weight = 0.5;
    float split_factor = 0.0;
    bool optimize = false;
};

inline std::vector<Cluster> clusterize(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
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

    if (options.optimize) {
        for (const auto &m : meshlet_result.meshlets) {
            meshopt::optimize_meshlet(
                std::span(&meshlet_result.vertex_indices[m.vertex_offset], m.vertex_count),
                std::span(&meshlet_result.local_triangles[m.triangle_offset], m.triangle_count)
            );
        }
    }

    for (const auto &meshlet : meshlet_result.meshlets) {
        Cluster cluster;

        // Map meshlet vertices to global vertex indices
        if (global_vertex_map.empty()) {
            cluster.vertex_indices.insert(
                cluster.vertex_indices.end(),
                meshlet_result.vertex_indices.begin() + meshlet.vertex_offset,
                meshlet_result.vertex_indices.begin() + meshlet.vertex_offset + meshlet.vertex_count);
        } else {
            cluster.vertex_indices.reserve(meshlet.vertex_count);
            for (size_t i = 0; i < meshlet.vertex_count; i++) {
                cluster.vertex_indices.push_back(global_vertex_map[meshlet_result.vertex_indices[meshlet.vertex_offset + i]]);
            }
        }

        // Remap triangles
        cluster.local_triangles.reserve(meshlet.triangle_count);
        const size_t triangle_start = meshlet.triangle_offset / 3;
        const size_t triangle_end = triangle_start + meshlet.triangle_count;
        for (size_t i = triangle_start; i < triangle_end; i++) {
            const glm::tvec3<uint8_t> meshlet_triangle = meshlet_result.local_triangles[i];
            cluster.local_triangles.emplace_back(meshlet_triangle);
        }

        cluster.relative_error = 0.0;
        cluster.uv_unwrapping = std::nullopt;

        clusters.push_back(std::move(cluster));
    }

    return clusters;
}

Clustering clusterize(const mesh::Simple3d& mesh, const ClusterOptions &options = {}) {
    const auto positions_f = to_approximate_normalized(mesh.positions);

    auto clusters = clusterize(mesh.triangles, positions_f, options, {});

    std::vector<glm::dvec3> positions(mesh.positions.begin(), mesh.positions.end());
    return Clustering{std::move(positions), std::move(clusters)};
}

namespace {
void materialize_cluster_positions(const Cluster& cluster, const std::span<const glm::dvec3> source_positions, std::vector<glm::dvec3>& target_positions) {
    target_positions.clear();
    target_positions.reserve(cluster.vertex_indices.size());
    for (const uint32_t vertex_index : cluster.vertex_indices) {
        target_positions.push_back(source_positions[vertex_index]);
    }
}

std::vector<glm::dvec3> materialize_cluster_positions(const Cluster &cluster, const std::span<const glm::dvec3> source_positions) {
    std::vector<glm::dvec3> target_positions;
    materialize_cluster_positions(cluster, source_positions, target_positions);
    return target_positions;
}
}

std::vector<Cluster> clusterize(const Cluster &cluster, const std::span<const glm::dvec3> positions, const ClusterOptions &options = {}) {
    const std::vector<glm::dvec3> positions_d = materialize_cluster_positions(cluster, positions);
    const std::vector<glm::vec3> positions_f = to_approximate_normalized(positions_d);

    return clusterize(
        cluster.local_triangles,
        positions_f,
        options,
        cluster.vertex_indices);
}

Clustering clusterize(const Clustering &input, const ClusterOptions &options = {}) {
    std::vector<Cluster> new_clusters;

    std::vector<glm::dvec3> positions_d;
    std::vector<glm::vec3> positions_f;
    for (const auto &cluster : input.clusters) {
        // Materialize cluster positions in float
        materialize_cluster_positions(cluster, input.positions, positions_d);
        to_approximate_normalized(positions_d, positions_f);

        // Sub-clusters using parent’s vertex mapping
        auto sub_clusters = clusterize(
            cluster.local_triangles,
            positions_f,
            options,
            cluster.vertex_indices);

        
        new_clusters.insert(new_clusters.end(),
                            std::make_move_iterator(sub_clusters.begin()),
                            std::make_move_iterator(sub_clusters.end()));
    }

    return Clustering{input.positions, std::move(new_clusters)};
}

