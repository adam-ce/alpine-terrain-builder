#pragma once

#include <variant>
#include <unordered_set>
#include <type_traits>
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

struct VertexLock {
    struct None {};
    struct Boundary {};
    struct BoundaryTriangles {};
    struct Custom {
        const std::span<const uint8_t> mask;
    };

    using Variant = std::variant<None, Boundary, BoundaryTriangles, Custom>;
    Variant v;

    VertexLock() : v(None{}) {}
    static VertexLock none() {
        return VertexLock{None{}};
    }
    static VertexLock boundary() {
        return VertexLock{Boundary{}};
    }
    static VertexLock boundaryTriangles() {
        return VertexLock{BoundaryTriangles{}};
    }
    static VertexLock custom(std::span<const std::uint8_t> mask) {
        return VertexLock{Custom{mask}};
    }

private:
    template <class T>
    explicit VertexLock(T t) : v(std::move(t)) {}
};

struct SimplifyOptions {
    float target_ratio = 0.5;
    float absolute_target_error = meshopt::NO_TARGET_ERROR;
    VertexLock vertex_lock = VertexLock::none();
};

[[nodiscard]]
Clustering simplify(
    const Clustering& original_clustering,
    const SimplifyOptions options = {}
) {
    Clustering simplified_clustering;
    simplified_clustering.texture = original_clustering.texture;
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

        // Set up vertex locks
        std::vector<uint8_t> vertex_locks;
        int lock_options = 0;
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VertexLock::None>) {
                // empty -> no locks
            } else if constexpr (std::is_same_v<T, VertexLock::Boundary>) {
                // use meshopt built-in boundary lock
                lock_options = meshopt_SimplifyLockBorder;
            } else if constexpr (std::is_same_v<T, VertexLock::BoundaryTriangles>) {
                const std::unordered_set<uint32_t> boundary_triangles = find_boundary_triangles(original_cluster.local_triangles);
                vertex_locks.resize(original_cluster.vertex_count(), 0);
                for (const uint32_t triangle_index : boundary_triangles) {
                    const glm::uvec3 &triangle = original_cluster.local_triangles[triangle_index];
                    for (uint8_t k = 0; k < 3; k++) {
                        const uint32_t vertex_index = triangle[k];
                        vertex_locks[vertex_index] |= meshopt_SimplifyVertex_Lock;
                    }
                }
            } else if constexpr (std::is_same_v<T, VertexLock::Custom>) {
                DEBUG_ASSERT(arg.mask.size() == original_vertex_count);
                vertex_locks.assign(arg.mask.begin(), arg.mask.end());
            }
        }, options.vertex_lock.v);

        // Perform simplification
        const size_t original_triangle_count = original_cluster.local_triangles.size();
        const size_t target_triangle_count = static_cast<size_t>(options.target_ratio * original_triangle_count);
        meshopt::SimplifyResult result = meshopt::simplify_with_attributes(
            original_cluster.local_triangles,
            cluster_positions_f,
            {},
            0,
            {},
            vertex_locks,
            target_triangle_count,
            relative_target_error,
            meshopt_SimplifyErrorAbsolute | lock_options);
        if (result.triangles.empty()) {
            // Simplification removed all triangles, go to next cluster
            continue;
        }

        // Create remap for vertex compaction
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
        // Update triangles with compacted vertex indices
        std::vector<glm::uvec3> local_triangles = std::move(result.triangles);
        for (glm::uvec3 &triangle : local_triangles) {
            for (uint8_t k = 0; k < 3; k++) {
                uint32_t &index = triangle[k];
                DEBUG_ASSERT(vertex_remap[index] != invalid_index);
                index = vertex_remap[index];
            }
        }

        std::vector<glm::dvec2> uvs;
        if (!original_cluster.uvs.empty()) {
            uvs.resize(new_vertex_count);
            // Add original vertex UVs
            for (uint32_t original_index = 0; original_index < original_vertex_count; original_index++) {
                const uint32_t new_index = vertex_remap[original_index];
                if (new_index == invalid_index) {
                    // vertex was removed during simplification
                    continue;
                }

                uvs[new_index] = original_cluster.uvs[original_index];
            }
        }
        
        // Make error absolute
        const float absolute_error = result.relative_error * (max_extents * 2);

        // Create new cluster
        Cluster simplified_cluster{
            .vertex_indices = std::move(vertex_indices),
            .local_triangles = std::move(local_triangles),
            .uvs = std::move(uvs),
            .absolute_error = absolute_error};
        validate(simplified_cluster, simplified_clustering.positions);
        simplified_clustering.clusters.push_back(std::move(simplified_cluster));
    }

    return simplified_clustering;
}
