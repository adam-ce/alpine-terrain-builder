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
        const size_t vertex_count = original_cluster.vertex_indices.size();

        // Materialize positions vector
        cluster_positions.clear();
        cluster_positions.reserve(vertex_count);
        for (const uint32_t vertex_index : original_cluster.vertex_indices) {
            cluster_positions.push_back(original_clustering.positions[vertex_index]);
        }

        cluster_positions_f.clear();
        cluster_positions_f.reserve(vertex_count);

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

        // Create new cluster vertices
        constexpr uint32_t invalid_index = UINT32_MAX;
        vertex_remap.assign(vertex_count, invalid_index);
        std::vector<uint32_t> vertex_indices;
        vertex_indices.reserve(vertex_count);

        for (const glm::uvec3 triangle : result.triangles) {
            for (uint8_t k = 0; k < 3; k++) {
                const uint32_t original_vertex_index = triangle[k];
                uint32_t &new_vertex_index = vertex_remap[original_vertex_index];
                if (new_vertex_index == invalid_index) {
                    const uint32_t global_vertex_index = original_cluster.vertex_indices[original_vertex_index];
                    new_vertex_index = vertex_indices.size();
                    vertex_indices.push_back(global_vertex_index);
                }
            }
        }
        
        // Localize triangles
        std::vector<glm::uvec3> local_triangles = std::move(result.triangles);
        for (glm::uvec3 &triangle : local_triangles) {
            for (uint8_t k = 0; k < 3; k++) {
                uint32_t& index = triangle[k];
                index = vertex_remap[index];
            }
        }

        // Make sure the result is manifold
        const size_t duplicated_vertices_start = cluster_mesh.vertex_count();
        make_manifold(result.triangles, cluster_positions);
        cluster_positions_f.clear(); // Invalidate to avoid misuse later
        // Update vertex indices to include duplicated vertices
        if (cluster_positions.size() > duplicated_vertices_start) {
            for (size_t local_index = duplicated_vertices_start; local_index < cluster_positions.size(); local_index++) {
                const size_t original_index = duplicate_vertices_start + local_index;
                vertex_indices.push_back(original_index);
            }
        }

        // Create new cluster
        Cluster simplified_cluster{
            .vertex_indices = std::move(vertex_indices),
            .local_triangles = std::move(local_triangles),
            .relative_error = result.relative_error};
        simplified_clustering.clusters.push_back(std::move(simplified_cluster));
    }

    return simplified_clustering;
}
