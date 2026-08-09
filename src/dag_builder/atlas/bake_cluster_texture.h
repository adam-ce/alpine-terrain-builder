#pragma once

#include <cstdint>
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
