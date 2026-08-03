    #pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

#include <glm/glm.hpp>

#include "cluster.h"
#include "variant_utils.h"

// Fixed texel budget per cluster.
struct ConstantQuality {
    uint32_t texels_per_cluster = 128;
};

// Dynamic texel budget relative to input textures.
struct RelativeQuality {};

struct BakeOptions {
    std::variant<ConstantQuality, RelativeQuality> mode = ConstantQuality{};
    uint32_t min_cluster_texture_size = 64;
    uint32_t max_cluster_texture_size = 512;
};

// Texels a merged cluster gets per triangle.
inline double texel_density(
    const BakeOptions &options,
    const uint32_t source_triangle_count,
    const double source_pixel_area) {
    const double source_density = source_pixel_area / source_triangle_count;

    return match(options.mode,
        [&](const ConstantQuality &mode) {
            const double texels_per_cluster = mode.texels_per_cluster;
            return std::min(texels_per_cluster * texels_per_cluster / MAX_TRIANGLES_PER_CLUSTER, source_density);
        },
        [&](const RelativeQuality &) {
            return source_density;
        });
}

// Resolution fitting the texel budget.
inline glm::uvec2 compute_bake_texture_size(const double target_pixel_area, const float utilization, const BakeOptions &options) {
    const double padded_area = target_pixel_area / std::max<double>(utilization, 1e-3);
    const uint32_t side = std::ceil(std::sqrt(padded_area));
    const uint32_t clamped_side = std::clamp(side, options.min_cluster_texture_size, options.max_cluster_texture_size);
    return glm::uvec2(clamped_side);
}

// Resolution fitting the texel budget.
inline glm::uvec2 compute_bake_texture_size(
    const double source_pixel_area,
    const uint32_t source_triangle_count,
    const uint32_t target_triangle_count,
    const float utilization,
    const BakeOptions &options) {
    const double density = texel_density(options, source_triangle_count, source_pixel_area);
    return compute_bake_texture_size(density * target_triangle_count, utilization, options);
}
