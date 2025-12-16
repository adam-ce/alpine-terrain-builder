#pragma once

#include <cmath>
#include <type_traits>
#include <vector>

#include <libassert/assert.hpp>

#include "clusterize.h"
#include "group.h"
#include "meshopt.h"
#include "utils.h"
#include "log.h"
#include "number_types.h"

namespace {
template <typename T, typename F>
constexpr inline T integral_lerp(T a, T b, F f) {
    static_assert(std::is_integral_v<T>, "lerp<T, F>: T must be an integral type");
    static_assert(std::is_arithmetic_v<F>, "lerp<T, F>: F must be a number type");

    DEBUG_ASSERT(f >= 0 && f <= 1);

    if constexpr (std::is_integral_v<F>) {
        DEBUG_ASSERT(f == 0 || f == 1);
        return f == 0 ? a : b;
    } else if constexpr (std::is_floating_point_v<F>) {
        DEBUG_ASSERT(f >= 0 && f <= 1);

        if (f <= F(0)) {
            return a;
        }
        if (f >= F(1)) {
            return b;
        }

        using Offset = next_precision_t<std::make_signed_t<T>>;
        const Offset range = static_cast<Offset>(b) - static_cast<Offset>(a);
        return a + static_cast<Offset>(std::round(range * f));
    } else {
        UNREACHABLE();
    }
}
}

Clustering split_each_into_equal_parts(const Clustering &input, const size_t num_parts = 2, const float uniformity_strength = 0.5) {
    if (num_parts == 0) {
        return {
            .positions = input.positions,
            .clusters = {}
        };
    }
    if (num_parts == 1) {
        return input;
    }
    DEBUG_ASSERT(uniformity_strength >= 0 && uniformity_strength <= 1);

    std::vector<Cluster> new_clusters;

    for (const auto& cluster : input.clusters) {
        const uint32_t triangle_count = cluster.local_triangles.size();
        const uint32_t vertex_count = cluster.vertex_indices.size();

        // Ensure we can meaningfully split triangles and vertices
        ASSERT(triangle_count >= num_parts && "Cluster has fewer triangles than requested parts");
        ASSERT(vertex_count >= num_parts && "Cluster has fewer vertices than requested parts");
        // TODO: ASSERT(vertex_count / num_parts <= ClusterOptions::MAX_VERTEX_LIMIT);

        auto floor4 = [](const uint32_t v) { return v & ~uint32_t(3); };

        const uint32_t target_triangle_count = static_cast<float>(triangle_count) / static_cast<float>(num_parts);
        const uint32_t base_min_triangles = triangle_count / (num_parts + 1) + 1;
        const uint32_t base_max_triangles = triangle_count / (num_parts - 1) - 1;
        const uint32_t min_triangles = integral_lerp(base_min_triangles, target_triangle_count, uniformity_strength);
        const uint32_t max_triangles = floor4(integral_lerp(base_max_triangles, target_triangle_count, uniformity_strength));

        LOG_INFO("Cluster with {} triangles: min_triangles={}, max_triangles={}",
                 triangle_count, min_triangles, max_triangles);
        LOG_INFO("Cluster with {} vertices", vertex_count);

        const ClusterOptions options{
            .min_triangles = min_triangles,
            .max_triangles = max_triangles,
            .cone_weight = 1.0,
            // .split_factor = 1.0,
        };

        std::vector<Cluster> split_clusters = clusterize(cluster, input.positions, options);
        // TODO: ASSERT(split_clusters.size() == num_parts);
        new_clusters.insert(
            new_clusters.end(),
            std::make_move_iterator(split_clusters.begin()),
            std::make_move_iterator(split_clusters.end()));
    }

    return Clustering{input.positions, std::move(new_clusters)};
}

Clustering split_each_into_equal_parts2(const Clustering &input, const size_t num_parts = 2) {
    if (num_parts == 0) {
        return {
            .positions = input.positions,
            .clusters = {}};
    }
    if (num_parts == 1) {
        return input;
    }

    validate(input);
    const Clustering intermediate = clusterize(input, ClusterOptions {
        .max_vertices = 64,
        .min_triangles = 42,
        .max_triangles = 126,
        .cone_weight = 1.0,
    });
    validate(intermediate);
    const Clustering output = group(intermediate, GroupOptions{
                                                      .clusters_per_group = (intermediate.clusters.size() + 1u) / 2u});

    DEBUG_ASSERT(output.clusters.size() == 2);

    return output;
}
