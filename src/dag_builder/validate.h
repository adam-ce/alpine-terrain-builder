#pragma once

#include <libassert/assert.hpp>

#include "cluster.h"

inline void validate(const Clustering &clustering) {
    ASSERT(!clustering.positions.empty(), "Clustering must have positions.");

    const uint32_t mesh_vertex_count = static_cast<uint32_t>(clustering.positions.size());
    const uint32_t cluster_count = static_cast<uint32_t>(clustering.clusters.size());

    for (uint32_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        const size_t cluster_vertex_count = cluster.vertex_indices.size();

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
    }
}
