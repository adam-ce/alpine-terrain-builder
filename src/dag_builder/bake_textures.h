#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "atlas/TextureBaker.h"
#include "cluster.h"
#include "enumerate.h"
#include "merge/clusters.h"
#include "opencv_utils.h"
#include "texture_sizing.h"

namespace detail {
// Triangles each texture serves.
inline std::vector<uint32_t> count_triangles_per_texture(const Clustering &clustering) {
    std::vector<uint32_t> counts(clustering.textures.size(), 0);
    for (const Cluster &cluster : clustering.clusters) {
        if (cluster.has_texture()) {
            counts[cluster.texture_id.value()] += cluster.triangle_count();
        }
    }
    return counts;
}
} // namespace detail

// Render every texture at the resolution its surviving triangles call for.
inline Clustering bake_textures(MergeResult merged, const BakeOptions &options) {
    Clustering clustering = std::move(merged.clustering);
    const std::vector<uint32_t> target_triangle_counts = detail::count_triangles_per_texture(clustering);

    for (const auto [texture_id, source_triangle_count] : enumerate(merged.source_triangle_counts)) {
        cv::Mat &texture = clustering.textures[texture_id];
        const glm::uvec2 texture_size = compute_bake_texture_size(
            glm::compMul(glm::dvec2(get_texture_size(texture))),
            source_triangle_count,
            target_triangle_counts[texture_id],
            1.0f,
            options);
        texture = rescale_texture(texture, texture_size);
    }

    for (const auto &[cluster_index, atlas] : merged.unbaked) {
        Cluster &cluster = clustering.clusters[cluster_index];
        const glm::uvec2 texture_size = compute_bake_texture_size(
            atlas.source_pixel_area(),
            atlas.source_triangle_count(),
            cluster.triangle_count(),
            atlas.utilization(),
            options);
        const uint32_t texture_id = clustering.textures.add(atlas.bake(texture_size));
        cluster.texture_id = texture_id;
    }

    return clustering;
}
