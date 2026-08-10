#pragma once

#include <algorithm>
#include <variant>
#include <unordered_set>
#include <type_traits>
#include <vector>
#include <span>
#include <optional>

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

// Target ratio used when neither a ratio nor an error target is specified
inline constexpr float DEFAULT_TARGET_RATIO = 0.5f;

// Controls how the input cluster's error is combined with the error introduced by this
// simplification when setting the simplified cluster's error.
enum class ErrorMode {
    Overwrite, // keep only this simplification's error, discarding the input error
    Add,       // add this simplification's error to the input error
    Max,       // keep the larger of the input error and this simplification's error
};

struct SimplifyOptions {
    std::optional<float> target_ratio;
    std::optional<float> absolute_target_error;
    VertexLock vertex_lock = VertexLock::none();
    float uv_weight = 0.5;
    ErrorMode error_mode = ErrorMode::Add;
    // Emit exactly one output cluster per input cluster
    bool preserve_cluster_count = false;

    // Default options apply a target ratio when no stop condition is specified.
    static SimplifyOptions defaults() {
        SimplifyOptions options;
        options.target_ratio = DEFAULT_TARGET_RATIO;
        return options;
    }
};

namespace detail {
    inline double combine_error(const ErrorMode mode, const double input_error, const double current_error) {
        switch (mode) {
        case ErrorMode::Overwrite:
            return current_error;
        case ErrorMode::Add:
            return input_error + current_error;
        case ErrorMode::Max:
            return std::max(input_error, current_error);
        }
        return current_error;
    }

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
    const SimplifyOptions options = SimplifyOptions::defaults()
) {
    Clustering simplified_clustering;
    simplified_clustering.textures = original_clustering.textures;
    simplified_clustering.positions = original_clustering.positions;

    std::vector<glm::dvec3> cluster_positions;
    std::vector<glm::vec3> cluster_positions_f;
    std::vector<glm::vec2> cluster_uvs_f;
    std::vector<uint32_t> vertex_remap;

    const float target_ratio = options.target_ratio.value_or(0.0f);
    const float absolute_target_error = options.absolute_target_error.value_or(meshopt::NO_TARGET_ERROR);

    for (const Cluster &original_cluster : original_clustering.clusters) {
        const size_t original_vertex_count = original_cluster.vertex_indices.size();

        // Materialize positions vector
        collect_cluster_positions(original_cluster, original_clustering.positions, cluster_positions);

        // Normalize positions and adjust target error accordingly
        radix::geometry::Aabb3d bounds;
        cluster_positions_f.clear();
        cluster_positions_f.reserve(original_vertex_count);
        to_approximate_normalized(cluster_positions, cluster_positions_f, &bounds);
        const float max_extents = glm::compMax(bounds.size()) / 2.0f;
        if (max_extents == 0.0f) {
            // Empty or degenerate cluster
            if (options.preserve_cluster_count) {
                simplified_clustering.clusters.push_back(original_cluster);
            }
            continue;
        }
        const float relative_target_error = absolute_target_error == meshopt::NO_TARGET_ERROR ?
            meshopt::NO_TARGET_ERROR : absolute_target_error / max_extents;

        // Prepare vertex attributes (uv)
        cluster_uvs_f.clear();
        to_approximate_normalized(original_cluster.uvs, cluster_uvs_f);
        std::span<const float> vertex_attributes = {};
        size_t vertex_attribute_stride = 0;
        std::vector<float> vertex_attribute_weights = {};
        if (options.uv_weight != 0.0f && original_cluster.is_textured()) {
            vertex_attributes = flatten(cluster_uvs_f);
            vertex_attribute_stride = sizeof(glm::vec2);
            vertex_attribute_weights = {options.uv_weight};
        }

        // Set up vertex locks
        std::vector<uint8_t> vertex_locks = detail::resolve_vertex_lock(options.vertex_lock, original_clustering, original_cluster);

        // Perform simplification
        const size_t original_triangle_count = original_cluster.local_triangles.size();
        size_t target_triangle_count = static_cast<size_t>(target_ratio * original_triangle_count);
        if (options.preserve_cluster_count) {
            // Keep at least one triangle so the cluster cannot disappear
            target_triangle_count = std::max(target_triangle_count, size_t{1});
        }
        meshopt::SimplifyResult result = meshopt::simplify_with_attributes(
            original_cluster.local_triangles,
            cluster_positions_f,
            vertex_attributes,
            vertex_attribute_stride,
            vertex_attribute_weights,
            vertex_locks,
            target_triangle_count,
            relative_target_error,
            meshopt_SimplifyErrorAbsolute);
        if (result.triangles.empty()) {
            // Simplification removed all triangles
            if (options.preserve_cluster_count) {
                simplified_clustering.clusters.push_back(original_cluster);
            }
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
        if (original_cluster.has_uvs()) {
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
        
        // Make error absolute and combine with the input cluster's error
        const double absolute_error = result.relative_error * max_extents;
        const double combined_error = detail::combine_error(options.error_mode, original_cluster.absolute_error, absolute_error);

        // Create new cluster
        Cluster simplified_cluster{
            .id = original_cluster.id,
            .vertex_indices = std::move(vertex_indices),
            .local_triangles = std::move(local_triangles),
            .uvs = std::move(uvs),
            .texture_id = original_cluster.texture_id,
            .absolute_error = combined_error
        };
        validate(simplified_cluster, simplified_clustering.positions);
        simplified_clustering.clusters.push_back(std::move(simplified_cluster));
    }

    if (options.preserve_cluster_count) {
        DEBUG_ASSERT(original_clustering.cluster_count() == simplified_clustering.cluster_count());
    }
    validate(simplified_clustering);
    return simplified_clustering;
}
