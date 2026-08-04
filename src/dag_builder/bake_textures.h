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

// Bake the deferred textures based on the given bake options.
inline Clustering bake_textures(MergeResult merged, const BakeOptions &options) {
    Clustering clustering = std::move(merged.clustering);
    const std::vector<uint32_t> target_triangle_counts = detail::count_triangles_per_texture(clustering);

    // Size every texture before rendering any, the budget is shared across the whole node.
    std::vector<glm::uvec2> sizes;
    sizes.reserve(merged.source_triangle_counts.size() + merged.unbaked.size());

    for (const auto [texture_id, source_triangle_count] : enumerate(merged.source_triangle_counts)) {
        const glm::dvec2 source_size = get_texture_size(clustering.textures[texture_id]);
        const double aspect = source_size.x / source_size.y;
        sizes.push_back(compute_bake_texture_size(
            glm::compMul(source_size),
            source_triangle_count,
            target_triangle_counts[texture_id],
            1.0,
            aspect,
            options));
    }

    const uint32_t reused_count = sizes.size();
    std::vector<uint32_t> baked_clusters;
    baked_clusters.reserve(merged.unbaked.size());

    for (const auto &[cluster_index, atlas] : merged.unbaked) {
        baked_clusters.push_back(cluster_index);
        sizes.push_back(compute_bake_texture_size(
            atlas.source_texel_area(),
            atlas.source_triangle_count(),
            clustering.clusters[cluster_index].triangle_count(),
            atlas.utilization(),
            atlas.aspect(),
            options));
    }

    detail::fit_node_budget(sizes, options.max_node_texels);

    for (const uint32_t texture_id : range(reused_count)) {
        cv::Mat &texture = clustering.textures[texture_id];
        texture = rescale_texture(texture, sizes[texture_id]);
    }

    for (const auto [baked_index, cluster_index] : enumerate(baked_clusters)) {
        const PackedAtlas &atlas = merged.unbaked.at(cluster_index);
        const uint32_t texture_id = clustering.textures.add(atlas.bake(sizes[reused_count + baked_index]));
        clustering.clusters[cluster_index].texture_id = texture_id;
    }

    return clustering;
}
