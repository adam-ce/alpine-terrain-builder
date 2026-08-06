#pragma once

#include <libassert/assert.hpp>

#include "cluster.h"
#include "utils.h"
#include "mesh/validate.h"

inline void validate(const Cluster &cluster, const std::span<const glm::dvec3> positions) {
    const size_t cluster_vertex_count = cluster.vertex_indices.size();
    const size_t mesh_vertex_count = positions.size();

    // Cluster must have vertices
    ASSERT(!cluster.vertex_indices.empty(), "Cluster has zero vertices");

    // Cluster must have triangles
    ASSERT(!cluster.local_triangles.empty(), "Cluster has zero triangles");

    // Validate vertex indices reference valid mesh vertices
    for (const uint32_t vertex_index : cluster.vertex_indices) {
        ASSERT(vertex_index < mesh_vertex_count, "cluster.vertex_indices contains out-of-range vertex index");
    }

    // Validate triangle indices reference cluster-local vertices
    for (const glm::uvec3 &triangle : cluster.local_triangles) {
        for (uint8_t corner = 0; corner < 3; corner++) {
            ASSERT(triangle[corner] < cluster_vertex_count,
                    "cluster.local_triangles refers to invalid local vertex index");
        }
    }

    // Materialize cluster mesh to validate mesh
    const mesh::Simple mesh = materialize_cluster(cluster, positions);
    mesh::validate_basic(mesh);
}

inline void validate(const Clustering &clustering) {
    // ASSERT(!clustering.positions.empty(), "Clustering must have positions.");

    const uint32_t cluster_count = static_cast<uint32_t>(clustering.clusters.size());

    for (uint32_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        validate(cluster, clustering.positions);
        ASSERT(!cluster.has_texture() || cluster.texture_id.value() < clustering.textures.size());
        ASSERT(cluster.has_texture() == cluster.has_uvs());
    }
}
