#pragma once

#include <vector>
#include <span>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>

#include "cluster.h"
#include "meshopt.h"
#include "utils.h"
#include "mesh/validate.h"
#include "validate.h"

struct SimplifyOptions {
    float target_ratio = 0.5;
    float absolute_target_error = meshopt::NO_TARGET_ERROR;
};

[[nodiscard]]
Clustering simplify(const Clustering& original_clustering, const SimplifyOptions options = {}) {
    Clustering simplified_clustering;
    simplified_clustering.positions = original_clustering.positions;

    std::vector<glm::dvec3> cluster_positions;
    std::vector<glm::vec3> cluster_positions_f;
    std::vector<uint32_t> vertex_remap;
    for (const Cluster &original_cluster : original_clustering.clusters) {
        const size_t original_vertex_count = original_cluster.vertex_indices.size();

        // Materialize positions vector
        cluster_positions.clear();
        cluster_positions.reserve(original_vertex_count);
        for (const uint32_t vertex_index : original_cluster.vertex_indices) {
            cluster_positions.push_back(original_clustering.positions[vertex_index]);
        }

        cluster_positions_f.clear();
        cluster_positions_f.reserve(original_vertex_count);

        // Normalize positions and adjust target error accordingly
        radix::geometry::Aabb3d bounds;
        to_approximate_normalized(cluster_positions, cluster_positions_f, &bounds);
        const float max_extents = glm::compMax(bounds.size());
        const float relative_target_error = options.absolute_target_error == meshopt::NO_TARGET_ERROR ?
            meshopt::NO_TARGET_ERROR : options.absolute_target_error / (max_extents * 2);

        // Perform simplification
        const size_t original_triangle_count = original_cluster.local_triangles.size();
        const size_t target_triangle_count = static_cast<size_t>(options.target_ratio * original_triangle_count);
        meshopt::SimplifyResult result = meshopt::simplify(
            original_cluster.local_triangles,
            cluster_positions_f,
            target_triangle_count,
            relative_target_error,
            meshopt_SimplifyLockBorder | meshopt_SimplifyErrorAbsolute);

        // Make sure the result is manifold
        make_manifold(result.triangles, cluster_positions);
        const std::span<const glm::dvec3> duplicated_vertices(
            cluster_positions.begin() + original_vertex_count,
            cluster_positions.size() - original_vertex_count);
        // Copy new vertices to clustering positions
        simplified_clustering.positions.insert(
            simplified_clustering.positions.end(),
            duplicated_vertices.begin(),
            duplicated_vertices.end());

        // Crete remap for vertex compaction
        constexpr uint32_t invalid_index = UINT32_MAX;
        vertex_remap.assign(cluster_positions.size(), invalid_index);
        uint32_t next_index = 0;
        for (const glm::uvec3 &triangle : result.triangles) {
            for (uint8_t k = 0; k < 3; k++) {
                const uint32_t original_vertex_index = triangle[k];
                uint32_t &new_vertex_index = vertex_remap[original_vertex_index];
                if (new_vertex_index == invalid_index) {
                    new_vertex_index = next_index;
                    next_index++;
                }
            }
        }
        for (size_t i = 0; i < duplicated_vertices.size(); i++) {
            const uint32_t original_vertex_index = original_vertex_count + i;
            const uint32_t new_vertex_index = vertex_remap[original_vertex_index];
            DEBUG_ASSERT(new_vertex_index != invalid_index);
        }
        const uint32_t new_vertex_count = next_index;

        // Create new cluster local to global mapping
        std::vector<uint32_t> vertex_indices;
        vertex_indices.resize(new_vertex_count);
        // Add original vertices to mapping
        for (uint32_t original_index = 0; original_index < original_vertex_count; original_index++) {
            const uint32_t new_index = vertex_remap[original_index];
            if (new_index == invalid_index) {
                // vertex was removed during simplification
                continue;
            }

            const uint32_t global_index = original_cluster.vertex_indices[original_index];
            vertex_indices[new_index] = global_index;
        }
        // Add duplicate vertices to mapping
        const uint32_t duplicated_vertex_offset = simplified_clustering.positions.size() - duplicated_vertices.size();
        for (uint32_t i = 0; i < duplicated_vertices.size(); i++) {
            const uint32_t original_index = original_vertex_count + i;
            const uint32_t new_index = vertex_remap[original_index];
            DEBUG_ASSERT(new_index != invalid_index);

            const uint32_t global_index = duplicated_vertex_offset + i;
            vertex_indices[new_index] = global_index;
        }
        // Update triangles with compacted vertex indices
        std::vector<glm::uvec3> local_triangles = std::move(result.triangles);
        for (glm::uvec3 &triangle : local_triangles) {
            for (uint8_t k = 0; k < 3; k++) {
                uint32_t &index = triangle[k];
                DEBUG_ASSERT(vertex_remap[index] != invalid_index);
                index = vertex_remap[index];
            }
        }
        
        // Create new cluster
        Cluster simplified_cluster{
            .vertex_indices = std::move(vertex_indices),
            .local_triangles = std::move(local_triangles),
            .relative_error = result.relative_error};
        validate(simplified_cluster, simplified_clustering.positions);
        simplified_clustering.clusters.push_back(std::move(simplified_cluster));
    }

    return simplified_clustering;
}

