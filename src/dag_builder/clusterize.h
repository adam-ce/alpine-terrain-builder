#pragma once

#include <vector>

#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "meshopt.h"
#include "utils.h"

struct ClusterOptions {
    uint32_t max_vertices = 256;
    uint32_t max_triangles = 256;
    float cone_weight = 0.5;
    float split_factor = 0.0;
    bool optimize = false;
};

namespace {
inline std::vector<Cluster> build_clusters_core(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const std::span<const uint32_t> global_vertex_map,
    const ClusterOptions &options) {
    std::vector<Cluster> clusters;

    meshopt::BuildMeshletsResult meshlet_result = meshopt::build_meshlets(
        triangles,
        positions,
        options.max_vertices,
        options.max_triangles,
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
}

Clustering clusterize(const mesh::Simple3d& mesh, const ClusterOptions &options = {}) {
    const auto positions_f = to_approximate_normalized(mesh.positions);

    auto clusters = build_clusters_core(mesh.triangles, positions_f, {}, options);

    std::vector<glm::dvec3> positions(mesh.positions.begin(), mesh.positions.end());
    return Clustering{std::move(positions), std::move(clusters)};
}

Clustering clusterize(const Clustering &input, const ClusterOptions &options = {}) {
    std::vector<Cluster> new_clusters;

    for (const auto& cluster : input.clusters) {
        // Build parent positions in float
        const std::vector<glm::vec3> positions_f = to_approximate_normalized(input.positions);

        // Sub-clusters using parent’s vertex mapping
        auto sub_clusters = build_clusters_core(
            cluster.local_triangles,
            positions_f,
            cluster.vertex_indices, // global map
            options);

        new_clusters.insert(new_clusters.end(),
                            std::make_move_iterator(sub_clusters.begin()),
                            std::make_move_iterator(sub_clusters.end()));
    }

    return Clustering{input.positions, std::move(new_clusters)};
}

