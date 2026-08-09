#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "cluster.h"
#include "mesh/geometry.h"
#include "opencv_utils.h"
#include "range_utils.h"
#include "variant_utils.h"

// Fixed texel budget per cluster.
struct ConstantQuality {
    uint32_t target_cluster_texels = 128 * 128;
};

// Dynamic texel budget relative to input textures.
struct RelativeQuality {};

struct TextureSizingOptions {
    std::variant<ConstantQuality, RelativeQuality> mode = ConstantQuality{};
    uint32_t max_node_texels = 4096 * 4096;
};


// Fraction of its texture a cluster's uvs reach.
[[nodiscard]]
inline double compute_utilization(const Cluster &cluster) {
    return sum(cluster.local_triangles, [&](const glm::uvec3 &triangle) {
        return compute_triangle_area(triangle, cluster.uvs);
    });
}

// Texels of its texture a cluster actually reaches.
[[nodiscard]]
inline double compute_texel_area(const Clustering &clustering, const uint32_t cluster_index) {
    const Cluster &cluster = clustering.clusters[cluster_index];
    if (!cluster.has_texture()) {
        return 0;
    }
    const glm::dvec2 texture_size = get_texture_size(clustering.textures[cluster.texture_id.value()]);
    return compute_utilization(cluster) * glm::compMul(texture_size);
}

// Texels per triangle the group's textured sources carry.
[[nodiscard]]
inline double compute_source_texel_density(const Clustering &source, const std::span<const uint32_t> cluster_indices) {
    double texel_area = 0;
    uint32_t triangle_count = 0;
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = source.clusters[cluster_index];
        if (!cluster.has_texture() || !cluster.has_uvs()) {
            continue;
        }
        texel_area += compute_texel_area(source, cluster_index);
        triangle_count += cluster.triangle_count();
    }

    if (triangle_count == 0) {
        return 0;
    }
    return texel_area / triangle_count;
}

// Texels a cluster should get per triangle.
[[nodiscard]]
inline double compute_target_texel_density(const TextureSizingOptions &options, const double source_texel_density) {
    return match(options.mode,
        [&](const ConstantQuality &mode) {
            const double target_cluster_texels = mode.target_cluster_texels;
            return std::min(target_cluster_texels / MAX_TRIANGLES_PER_CLUSTER, source_texel_density);
        },
        [&](const RelativeQuality &) {
            return source_texel_density;
        });
}

// Smallest resolution holding the given texels at the given shape.
[[nodiscard]]
inline glm::uvec2 compute_size_from_area(const double area, const double aspect) {
    const glm::dvec2 size(std::sqrt(area * aspect), std::sqrt(area / aspect));
    return glm::max(glm::uvec2(glm::ceil(size)), glm::uvec2(1));
}

// Minimum utilization value so a collapsed unwrap cannot demand an unbounded texture.
inline constexpr double MIN_TEXTURE_COVERAGE = 0.1;

// What the clusters of one output texture demand it to carry.
struct TextureDemand {
    double texels = 0; // texels they want on the surface
    double coverage = 0; // fraction of the texture their uvs reach
};

// Compute size of a texture to hold the demanded texels.
[[nodiscard]]
inline glm::uvec2 compute_target_size(const TextureDemand &demand, const double aspect) {
    const double area = demand.texels / std::max(demand.coverage, MIN_TEXTURE_COVERAGE);
    return compute_size_from_area(area, aspect);
}

namespace detail {
inline void fit_node_budget(const std::span<glm::uvec2> sizes, const uint32_t max_node_texels) {
    const double requested_texels = sum(sizes, [](const glm::uvec2& size) {
        return glm::compMul(glm::dvec2(size));
    });
    if (requested_texels <= max_node_texels) {
        return;
    }

    const double scale = std::sqrt(max_node_texels / requested_texels);
    for (glm::uvec2 &size : sizes) {
        const glm::dvec2 scaled = glm::dvec2(size) * scale;
        size = glm::max(glm::uvec2(glm::ceil(scaled)), glm::uvec2(1));
    }
}
}
