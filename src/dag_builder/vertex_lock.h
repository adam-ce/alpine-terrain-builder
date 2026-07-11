#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "TinyVector.h"
#include "cluster.h"
#include "mesh/triangle_compare.h"
#include "simplify.h"
#include "utils.h"
#include "VertexInCluster.h"


// Signed distance from a point to an axis-aligned box: negative inside (distance to the
// nearest face), positive outside (euclidean distance to the surface), zero on a face.
inline double signed_distance_to_bounds(const radix::geometry::Aabb3d &bounds, const glm::dvec3 &point) {
    const glm::dvec3 gaps = glm::max(bounds.min - point, point - bounds.max);
    const double outside = glm::length(glm::max(gaps, glm::dvec3(0.0)));
    const double inside = glm::min(glm::compMax(gaps), 0.0);
    return outside + inside;
}

inline std::vector<uint8_t> find_vertices_to_lock(const Clustering &clustering, const radix::geometry::Aabb3d &node_bounds) {
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

    // Lock boundary vertices on or beyond a node face, as these are shared with
    // neighbouring nodes.
    const double lock_margin = glm::compMax(node_bounds.size()) * 1e-3;
    for (const uint32_t vertex_index : boundary_vertices) {
        const glm::dvec3 &position = clustering.positions[vertex_index];
        if (signed_distance_to_bounds(node_bounds, position) >= -lock_margin) {
            vertices_to_lock.insert(vertex_index);
        }
    }

    // Find all vertices shared between at least 2 clusters
    std::vector<TinyVector<VertexInCluster>> cluster_membership(vertex_count);
    for (uint32_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        for (const auto [local_vertex_index, vertex_index] : enumerate(cluster.vertex_indices)) {
            TinyVector<VertexInCluster> &membership = cluster_membership[vertex_index];
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
