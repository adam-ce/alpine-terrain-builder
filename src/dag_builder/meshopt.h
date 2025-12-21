#pragma once

#include <span>
#include <vector>

#include <libassert/assert.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <meshoptimizer.h>

#include "utils.h"

namespace meshopt {

inline constexpr size_t NO_TARGET_COUNT = 0;
inline constexpr float NO_TARGET_ERROR = FLT_MAX;

struct [[nodiscard]] SimplifyResult {
    std::vector<glm::uvec3> triangles;
    float relative_error = 0.0f;
};

inline SimplifyResult simplify(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const size_t target_triangle_count,
    const float target_error,
    const uint32_t options = 0) {
    const size_t vertex_count = positions.size();
    const size_t triangle_count = triangles.size();
    const size_t index_count = triangle_count * 3;
    const size_t positions_stride = sizeof(glm::vec3);
    const size_t target_index_count = target_triangle_count * 3;

    SimplifyResult result;
    result.triangles.resize(triangle_count);

    // Flatten spans
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);
    const std::span<uint32_t> result_indices = flatten(result.triangles);

    const size_t new_index_count = meshopt_simplify(
        result_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        vertex_count,
        positions_stride,
        target_index_count,
        target_error,
        options,
        &result.relative_error);

    // Resize to match returned number of indices
    const size_t new_triangle_count = new_index_count / 3;
    result.triangles.resize(new_triangle_count);

    return result;
}

inline SimplifyResult simplify_with_attributes(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const std::span<const float> vertex_attributes,
    const size_t vertex_attribute_stride,
    const std::span<const float> vertex_attribute_weights,
    const std::span<const uint8_t> vertex_locks,
    const size_t target_triangle_count,
    const float target_error,
    const uint32_t options) {
    const size_t vertex_count = positions.size();
    const size_t triangle_count = triangles.size();
    const size_t index_count = triangle_count * 3;
    const size_t attribute_count = vertex_attribute_weights.size();
    const size_t positions_stride = sizeof(glm::vec3);
    const size_t target_index_count = target_triangle_count * 3;

    DEBUG_ASSERT(vertex_attributes.size() >= vertex_count * (vertex_attribute_stride / sizeof(float)));
    DEBUG_ASSERT(vertex_locks.size() == vertex_count);

    SimplifyResult result;
    result.triangles.resize(triangle_count);

    // Flatten spans
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);
    const std::span<uint32_t> result_indices = flatten(result.triangles);

    const size_t new_index_count = meshopt_simplifyWithAttributes(
        result_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        vertex_count,
        positions_stride,
        vertex_attributes.data(),
        vertex_attribute_stride,
        vertex_attribute_weights.data(),
        attribute_count,
        vertex_locks.data(),
        target_index_count,
        target_error,
        options,
        &result.relative_error);

    // Resize to match returned number of indices
    const size_t new_triangle_count = new_index_count / 3;
    result.triangles.resize(new_triangle_count);

    return result;
}

struct [[nodiscard]] BuildMeshletsResult {
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> vertex_indices;  // flat vertex indices for all meshlets
    std::vector<glm::tvec3<uint8_t>> local_triangles; // flat triangle indices for all meshlets
};

inline BuildMeshletsResult build_meshlets(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const uint32_t max_vertices,
    const uint32_t min_triangles,
    const uint32_t max_triangles,
    const float cone_weight = 0.5f,
    const float split_factor = 0.0f) {
    ASSERT(max_vertices <= 256, "Max vertices per cluster must be less than 256.");

    // Flatten inputs
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);

    // Compute maximum number of meshlets needed
    const size_t index_count = indices.size();
    const uint32_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, max_triangles);

    // Allocate buffers
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<glm::tvec3<uint8_t>> meshlet_triangles(max_meshlets * max_triangles);
    std::span<uint8_t> meshlet_indices = flatten(meshlet_triangles);

    // Build meshlets
    const size_t meshlet_count = meshopt_buildMeshletsFlex(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        meshlet_vertices.size(),
        sizeof(glm::vec3),
        max_vertices,
        min_triangles,
        max_triangles,
        cone_weight,
        split_factor);
    ASSERT(meshlet_count <= max_meshlets);

    // Trim meshlet output buffer
    meshlets.resize(meshlet_count);

    // Convert index offsets into triangle offsets
    for (meshopt_Meshlet& meshlet : meshlets) {
        DEBUG_ASSERT(meshlet.triangle_offset % 3 == 0);
        meshlet.triangle_offset /= 3;
    }

    // Trim unused vertex and triangle buffers
    if (meshlet_count > 0) {
        const meshopt_Meshlet &last = meshlets[meshlet_count - 1];
        const size_t full_vertex_count = last.vertex_offset + last.vertex_count;
        const size_t full_triangle_count = last.triangle_offset + last.triangle_count;
        ASSERT(meshlet_vertices.size() >= full_vertex_count);
        ASSERT(meshlet_triangles.size() >= full_triangle_count);
        meshlet_vertices.resize(full_vertex_count);
        meshlet_triangles.resize(full_triangle_count);
    }

    return BuildMeshletsResult{
        std::move(meshlets),
        std::move(meshlet_vertices),
        std::move(meshlet_triangles)};
}

inline void optimize_meshlet(
    const std::span<uint32_t> vertex_indices,
    const std::span<glm::tvec3<uint8_t>> triangles) {
    const std::span<uint8_t> indices = flatten(triangles);

    meshopt_optimizeMeshlet(
        vertex_indices.data(),
        indices.data(),
        triangles.size(),
        vertex_indices.size());
}

struct [[nodiscard]] PartitionClustersResult {
    std::vector<uint32_t> cluster_partitions;
    size_t partition_count = 0;
};

inline PartitionClustersResult partition_clusters(
    const std::span<const uint32_t> cluster_indices,       // flattened vertex indices of all clusters
    const std::span<const uint32_t> cluster_vertex_counts, // number of vertices per cluster
    const std::span<const glm::vec3> vertex_positions,
    const uint32_t clusters_per_partition) {
    ASSERT(clusters_per_partition > 0, "clusters_per_partition must be > 0");

    // Allocate cluster to partition map
    const size_t cluster_count = cluster_vertex_counts.size();
    std::vector<uint32_t> partitions(cluster_count);

    const size_t total_index_count = std::accumulate(cluster_vertex_counts.begin(), cluster_vertex_counts.end(), 0u);
    const std::span<const float> positions_flat = flatten(vertex_positions);

    const size_t partition_count = meshopt_partitionClusters(
        partitions.data(),
        cluster_indices.data(),
        total_index_count,
        cluster_vertex_counts.data(),
        cluster_count,
        positions_flat.data(),
        vertex_positions.size(),
        sizeof(glm::vec3),
        clusters_per_partition
    );

    return PartitionClustersResult{std::move(partitions), partition_count};
}

} // namespace meshopt
