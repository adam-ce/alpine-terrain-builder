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
#include "mesh/boundary.h"
#include "validate.h"
#include "range_utils.h"

struct VertexLock {
    struct None {};
    struct Boundary {};
    struct BoundaryTriangles {};
    struct Mask {
        const std::span<const uint8_t> mask;
    };

    using Variant = std::variant<None, Boundary, BoundaryTriangles, Mask>;
    Variant v;

    VertexLock() : v(None{}) {}
    static VertexLock none() {
        return VertexLock{None{}};
    }
    static VertexLock boundary() {
        return VertexLock{Boundary{}};
    }
    static VertexLock boundary_triangles() {
        return VertexLock{BoundaryTriangles{}};
    }
    static VertexLock mask(const std::span<const std::uint8_t> mask) {
        return VertexLock{Mask{.mask = mask}};
    }

    static inline constexpr const uint8_t UNLOCKED = 0;
    static inline constexpr const uint8_t LOCKED = meshopt_SimplifyVertex_Lock;

private:
    template <class T>
    explicit VertexLock(T t) : v(std::move(t)) {}
};

struct SimplifyOptions {
    float target_ratio = 0.5;
    float absolute_target_error = meshopt::NO_TARGET_ERROR;
    VertexLock vertex_lock = VertexLock::none();
};

namespace detail {
    inline std::vector<uint8_t> resolve_vertex_lock(const VertexLock& v, const Clustering& clustering, const Cluster& cluster) {
        constexpr const uint8_t UNLOCKED = VertexLock::UNLOCKED;
        constexpr const uint8_t LOCKED = VertexLock::LOCKED;
        std::vector<uint8_t> vertex_lock;

        std::visit([&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VertexLock::None>) {
                // Nothing to do
            } else if constexpr (std::is_same_v<T, VertexLock::Boundary>) {
                mesh::build_boundary_vertex_mask<uint8_t>(cluster.local_triangles, cluster.vertex_count(), vertex_lock, LOCKED, UNLOCKED);
            } else if constexpr (std::is_same_v<T, VertexLock::BoundaryTriangles>) {
                const std::unordered_set<uint32_t> boundary_triangles = mesh::find_boundary_triangles(cluster.local_triangles);
                vertex_lock.resize(cluster.vertex_count(), UNLOCKED);
                for (const uint32_t triangle_index : boundary_triangles) {
                    const glm::uvec3 &triangle = cluster.local_triangles[triangle_index];
                    for (uint8_t k = 0; k < 3; k++) {
                        const uint32_t vertex_index = triangle[k];
                        vertex_lock[vertex_index] = LOCKED;
                    }
                }
            } else if constexpr (std::is_same_v<T, VertexLock::Mask>) {
                ASSERT(arg.mask.size() == clustering.vertex_count());
                vertex_lock.resize(cluster.vertex_count(), UNLOCKED);
                for (const uint32_t local_vertex_index : range(cluster.vertex_count())) {
                    const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
                    const uint8_t mask_value = arg.mask[global_vertex_index];
                    DEBUG_ASSERT(mask_value == LOCKED || mask_value == UNLOCKED);
                    vertex_lock[local_vertex_index] = mask_value;
                }
            }
        }, v.v);
        return vertex_lock;
    }
}

[[nodiscard]]
inline Clustering simplify(
    const Clustering& original_clustering,
    const SimplifyOptions options = {}
) {
    Clustering simplified_clustering;
    simplified_clustering.textures = original_clustering.textures;
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
        std::vector<uint8_t> vertex_locks = detail::resolve_vertex_lock(options.vertex_lock, original_clustering, original_cluster);

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
            meshopt_SimplifyErrorAbsolute);
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
            .texture_id = original_cluster.texture_id,
            .absolute_error = absolute_error
        };
        validate(simplified_cluster, simplified_clustering.positions);
        simplified_clustering.clusters.push_back(std::move(simplified_cluster));
    }

    return simplified_clustering;
}
