#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "atlas/reprojection.h"
#include "cluster.h"
#include "enumerate.h"
#include "range_utils.h"
#include "uv/atlas.h"

// Adopt the atlas layout, duplicating the cluster's seam vertices.
[[nodiscard]]
inline Cluster apply_atlas(const Cluster &cluster, uv::Atlas atlas) {
    Cluster applied = cluster;

    applied.vertex_indices = transform_vector(atlas.vertex_map, [&](const uint32_t local) {
        return cluster.vertex_indices[local];
    });
    applied.local_triangles = std::move(atlas.triangles);
    applied.uvs = std::move(atlas.uvs);
    return applied;
}

namespace detail {
constexpr uint32_t no_atlas_vertex = std::numeric_limits<uint32_t>::max();

// Rebuild one cluster from its slice of a node atlas, renumbering the atlas vertices it
// touches. A seam duplicates a vertex, so two atlas vertices can map to one clustering vertex.
[[nodiscard]]
inline Cluster take_atlas_slice(
    const Cluster &cluster,
    const uv::Atlas &atlas,
    const uint32_t triangle_offset,
    const std::span<uint32_t> vertex_remap /* scratch buffer */) {
    Cluster applied = cluster;
    applied.vertex_indices.clear();
    applied.local_triangles.clear();
    applied.uvs.clear();

    for (const uint32_t i : range(cluster.triangle_count())) {
        const glm::uvec3 &atlas_triangle = atlas.triangles[triangle_offset + i];
        glm::uvec3 local;
        for (const uint8_t corner : range<uint8_t>(3)) {
            const uint32_t atlas_vertex = atlas_triangle[corner];
            uint32_t &local_vertex = vertex_remap[atlas_vertex];
            if (local_vertex == no_atlas_vertex) {
                local_vertex = applied.vertex_indices.size();
                applied.vertex_indices.push_back(atlas.vertex_map[atlas_vertex]);
                applied.uvs.push_back(atlas.uvs[atlas_vertex]);
            }
            local[corner] = local_vertex;
        }
        applied.local_triangles.push_back(local);
    }

    for (const uint32_t i : range(cluster.triangle_count())) {
        const glm::uvec3 &atlas_triangle = atlas.triangles[triangle_offset + i];
        for (const uint8_t corner : range<uint8_t>(3)) {
            vertex_remap[atlas_triangle[corner]] = no_atlas_vertex;
        }
    }
    return applied;
}
} // namespace detail

// Give each cluster the uvs a node atlas laid out for it. Clusters must arrive in the order
// their triangles were handed to the unwrap, since that is how the atlas is sliced.
inline void apply_node_atlas(
    Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const uv::Atlas &atlas) {
    std::vector<uint32_t> vertex_remap(atlas.uvs.size(), detail::no_atlas_vertex);

    uint32_t triangle_offset = 0;
    for (const uint32_t cluster_index : cluster_indices) {
        Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t triangle_count = cluster.triangle_count();
        cluster = detail::take_atlas_slice(cluster, atlas, triangle_offset, vertex_remap);
        triangle_offset += triangle_count;
    }
    DEBUG_ASSERT(triangle_offset == atlas.triangles.size());
}

struct BakeTextureOptions {
    CorrespondenceOptions correspondence = {};
    ReprojectionOptions reprojection = {};
};

namespace detail {
// The surface a bake renders into, gathered from one or more clusters.
struct BakeTarget {
    std::vector<glm::uvec3> global_triangles; // indices into the clustering's positions
    std::vector<glm::uvec3> local_triangles; // indices into positions
    std::vector<glm::dvec2> uvs; // per local vertex
    std::vector<glm::dvec3> positions; // per local vertex

    void append(const Cluster &cluster, const std::span<const glm::dvec3> clustering_positions) {
        const uint32_t vertex_offset = this->positions.size();

        for (const glm::uvec3 &local : cluster.local_triangles) {
            this->global_triangles.push_back(cluster.global_triangle(local));
            this->local_triangles.push_back(local + glm::uvec3(vertex_offset));
        }
        for (const uint32_t global : cluster.vertex_indices) {
            this->positions.push_back(clustering_positions[global]);
        }
        this->uvs.insert(this->uvs.end(), cluster.uvs.begin(), cluster.uvs.end());
    }
};

// Pull every texel of the target back through the source surface.
[[nodiscard]]
inline cv::Mat bake_texture(
    const BakeTarget &target,
    const std::span<const glm::dvec3> positions,
    const BakeSource &source,
    const glm::uvec2 size,
    const BakeTextureOptions &options) {
    const Correspondence correspondence =
        find_source_triangles(source.triangles, target.global_triangles, positions, options.correspondence);
    const std::vector<ReprojectionTriangle> reprojection = build_reprojection_triangles(
        target.local_triangles, target.uvs, target.positions, correspondence, source);

    TextureReprojector reprojector(size, CV_8UC3, options.reprojection);
    return reprojector.render(source.images, reprojection);
}
} // namespace detail

// Render the texture of a single cluster.
[[nodiscard]]
inline cv::Mat bake_cluster_texture(
    const Cluster &cluster,
    const std::span<const glm::dvec3> positions,
    const BakeSource &source,
    const glm::uvec2 size,
    const BakeTextureOptions &options) {
    detail::BakeTarget target;
    target.append(cluster, positions);
    return detail::bake_texture(target, positions, source, size, options);
}

// Render one texture covering several clusters, which must already carry uvs addressing it.
[[nodiscard]]
inline cv::Mat bake_clusters_texture(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const BakeSource &source,
    const glm::uvec2 size,
    const BakeTextureOptions &options) {
    detail::BakeTarget target;
    for (const uint32_t cluster_index : cluster_indices) {
        target.append(clustering.clusters[cluster_index], clustering.positions);
    }
    return detail::bake_texture(target, clustering.positions, source, size, options);
}
