#pragma once

#include <cstdint>
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

// Render the cluster's texture by pulling every texel back through the source surface.
[[nodiscard]]
inline cv::Mat bake_cluster_texture(
    const Cluster &cluster,
    const std::span<const glm::dvec3> positions,
    const BakeSource &source,
    const glm::uvec2 size,
    const BakeTextureOptions &options) {
    // The correspondence walks the source, so it needs the cluster in the global vertex space.
    const std::vector<glm::uvec3> global_triangles = transform_vector(cluster.local_triangles, [&](const glm::uvec3 &local) {
        return cluster.global_triangle(local);
    });
    const Correspondence correspondence =
        find_source_triangles(source.triangles, global_triangles, positions, options.correspondence);

    // The bake needs the cluster's own space, where a seam vertex carries its own uv.
    const std::vector<glm::dvec3> local_positions = transform_vector(cluster.vertex_indices, [&](const uint32_t global) {
        return positions[global];
    });
    const std::vector<ReprojectionTriangle> reprojection = build_reprojection_triangles(
        cluster.local_triangles, cluster.uvs, local_positions, correspondence, source);

    TextureReprojector reprojector(size, CV_8UC3, options.reprojection);
    return reprojector.render(source.images, reprojection);
}
