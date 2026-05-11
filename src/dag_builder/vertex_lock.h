#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <glm/vec3.hpp>
#include <libassert/assert.hpp>

#include "TinyVector.h"
#include "cluster.h"
#include "mesh/bounds.h"
#include "mesh/triangle_compare.h"
#include "simplify.h"
#include "utils.h"

namespace detail {
inline radix::geometry::Aabb3d calculate_bounds(const Clustering &clustering) {
    return mesh::calculate_bounds(clustering.positions);
}

struct VertexInCluster {
    uint32_t cluster_index;
    uint32_t local_vertex_index;
};
}

inline std::vector<uint8_t> find_vertices_to_lock(const Clustering &clustering) {
    // Lock every triangle where any vertex is either on the border and near the bounds or shared between clusters.
    const uint32_t cluster_count = clustering.cluster_count();
    const uint32_t vertex_count = clustering.vertex_count();
    std::unordered_set<uint32_t> vertices_to_lock;

    // Find border vertices (vertices that are part of at least one boundary triangle)
    std::unordered_set<uint32_t> boundary_vertices;
    boundary_vertices.reserve(vertex_count);
    std::unordered_set<uint32_t> cluster_boundary_vertices;
    for (const auto &[cluster_index, cluster] : enumerate(clustering.clusters)) {
        cluster_boundary_vertices.clear();
        mesh::find_boundary_vertices(cluster.local_triangles, cluster_boundary_vertices);

        for (const uint32_t local_vertex_index : cluster_boundary_vertices) {
            const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
            boundary_vertices.insert(global_vertex_index);
        }
    }

    // Calculate safe bounds excluding the outer boundary of the mesh
    const radix::geometry::Aabb3d bounds = detail::calculate_bounds(clustering);
    const glm::dvec3 center = bounds.centre();
    const glm::dvec3 extents = bounds.size() / 2.0;
    const glm::dvec3 unlocked_extents = extents * 0.99;
    const radix::geometry::Aabb3d unlocked_bounds(center - unlocked_extents, center + unlocked_extents);

    // Lock vertices outside safe bounds that are on the boundary
    for (const uint32_t vertex_index : boundary_vertices) {
        // Lock only vertices outside the unlocked bounds
        const glm::dvec3 &position = clustering.positions[vertex_index];
        if (!unlocked_bounds.contains(position)) {
            vertices_to_lock.insert(vertex_index);
        }
    }

    // Find all vertices shared between at least 2 clusters
    std::vector<TinyVector<detail::VertexInCluster>> cluster_membership(vertex_count);
    for (uint32_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        for (const auto [local_vertex_index, vertex_index] : enumerate(cluster.vertex_indices)) {
            TinyVector<detail::VertexInCluster> &membership = cluster_membership[vertex_index];
            membership.emplace_back(cluster_index, local_vertex_index);
        }
    }
    for (const auto &[vertex_index, membership] : enumerate(cluster_membership)) {
        const size_t num_clusters = membership.size();
        if (membership.size() <= 1) {
            continue;
        }

        // Make sure the vertex is not just duplicated in a single cluster
        const uint32_t first_cluster_index = membership[0].cluster_index;
        bool all_from_same = true;
        for (uint32_t i = 1; i < num_clusters; i++) {
            if (membership[i].cluster_index != first_cluster_index) {
                all_from_same = false;
                break;
            }
        }
        if (all_from_same) {
            continue;
        }

        // Encountered a shared vertex
        // Get global vertex
        const Cluster &first_cluster = clustering.clusters[membership[0].cluster_index];
        const uint32_t global_vertex_index = first_cluster.vertex_indices[membership[0].local_vertex_index];
        for (uint32_t i = 1; i < num_clusters; i++) {
            const auto [cluster_index, local_vertex_index] = membership[i];
            const Cluster &cluster = clustering.clusters[cluster_index];
            const uint32_t other_global_vertex_index = cluster.vertex_indices[local_vertex_index];
            DEBUG_ASSERT(global_vertex_index == other_global_vertex_index);
        }

        // Check if on the boundary
        if (!boundary_vertices.contains(global_vertex_index)) {
            continue;
        }

        // If its also on the boundary mark as locked
        vertices_to_lock.insert(global_vertex_index);
    }

    // Allocate vertex lock buffer
    std::vector<uint8_t> vertex_lock(vertex_count, VertexLock::UNLOCKED);
    for (const uint32_t vertex_index : vertices_to_lock) {
        vertex_lock[vertex_index] = VertexLock::LOCKED;
    }

    return vertex_lock;
}
