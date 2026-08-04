#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <variant>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "cluster.h"
#include "range_utils.h"
#include "variant_utils.h"

// Fixed texel budget per cluster.
struct ConstantQuality {
    uint32_t target_cluster_texels = 128 * 128;
};

// Dynamic texel budget relative to input textures.
struct RelativeQuality {};

struct BakeOptions {
    std::variant<ConstantQuality, RelativeQuality> mode = ConstantQuality{};
    uint32_t max_node_texels = 4096 * 4096;
};

namespace detail {
// Smallest resolution holding the given texels at the given shape.
inline glm::uvec2 compute_size_from_area(const double area, const double aspect) {
    const glm::dvec2 size(std::sqrt(area * aspect), std::sqrt(area / aspect));
    return glm::max(glm::uvec2(glm::ceil(size)), glm::uvec2(1));
}

// Texels a merged cluster gets per triangle.
inline double texel_density(
    const BakeOptions &options,
    const uint32_t source_triangle_count,
    const double source_pixel_area) {
    const double source_density = source_pixel_area / source_triangle_count;

    return match(options.mode,
        [&](const ConstantQuality &mode) {
            const double target_cluster_texels = mode.target_cluster_texels;
            return std::min(target_cluster_texels / MAX_TRIANGLES_PER_CLUSTER, source_density);
        },
        [&](const RelativeQuality &) {
            return source_density;
        });
}

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

// Resolution fitting the texel budget.
inline glm::uvec2 compute_bake_texture_size(
    const double source_pixel_area,
    const uint32_t source_triangle_count,
    const uint32_t target_triangle_count,
    const double utilization,
    const double aspect,
    const BakeOptions &options) {
    if (source_triangle_count == 0 || target_triangle_count == 0) {
        return glm::uvec2(1);
    }

    const double density = detail::texel_density(options, source_triangle_count, source_pixel_area);
    const double atlas_area = density * target_triangle_count / std::max(utilization, 1e-3);
    return detail::compute_size_from_area(atlas_area, aspect);
}
