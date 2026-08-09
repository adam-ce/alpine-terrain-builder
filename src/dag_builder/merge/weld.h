#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "containers/UnionFind.h"
#include "cluster.h"
#include "compact.h"
#include "enumerate.h"
#include "log.h"
#include "mesh/connectivity/adjacency.h"
#include "spatial_lookup/Hashmap.h"
#include "validate.h"

namespace detail {

// Maps each position onto the welded position of its transitive epsilon neighbourhood.
inline std::vector<uint32_t> build_welding_map(const std::span<const glm::dvec3> positions, const double epsilon) {
    const uint32_t position_count = positions.size();

    spatial_lookup::Hashmap3d<uint32_t> spatial_map(epsilon * 5);
    for (const auto &[position_index, position] : enumerate<uint32_t>(positions)) {
        spatial_map.insert(position, position_index);
    }

    UnionFind_<false, uint32_t, uint32_t> neighbourhoods(position_count);
    std::vector<uint32_t> matches;

    for (const auto &[position_index, position] : enumerate<uint32_t>(positions)) {
        // Find all positions within epsilon.
        matches.clear();
        spatial_map.find_all_near(position, epsilon, matches);

        for (const uint32_t match_index : matches) {
            // Skip the position itself and any pair already covered in reverse
            if (match_index <= position_index) {
                continue;
            }

            neighbourhoods.make_union(position_index, match_index);
        }
    }

    return neighbourhoods.get_set_labels();
}

inline std::vector<glm::dvec3> build_welded_positions(
    const std::span<const glm::dvec3> positions,
    const std::span<const uint32_t> position_to_welded,
    const uint32_t welded_position_count,
    const bool average_positions) {
    std::vector<glm::dvec3> welded_positions(welded_position_count, glm::dvec3(0.0));

    if (!average_positions) {
        for (const auto &[position_index, position] : enumerate(positions)) {
            welded_positions[position_to_welded[position_index]] = position;
        }

        return welded_positions;
    }

    std::vector<uint32_t> counts(welded_position_count, 0);
    for (const auto &[position_index, position] : enumerate(positions)) {
        const uint32_t welded_index = position_to_welded[position_index];
        welded_positions[welded_index] += position;
        counts[welded_index]++;
    }

    for (const auto &[welded_index, count] : enumerate(counts)) {
        DEBUG_ASSERT(count > 0);
        welded_positions[welded_index] /= static_cast<double>(count);
    }

    return welded_positions;
}

inline void filter_degenerate_triangles(Clustering &clustering) {
    for (Cluster &cluster : clustering.clusters) {
        const size_t removed = std::erase_if(cluster.local_triangles, [&](const glm::uvec3 &local_triangle) {
            const glm::uvec3 global_triangle(
                cluster.vertex_indices[local_triangle.x],
                cluster.vertex_indices[local_triangle.y],
                cluster.vertex_indices[local_triangle.z]);

            return mesh::is_degenerate(global_triangle);
        });

        // Compact vertices if we found any.
        if (removed > 0) {
            compact_cluster_inplace(cluster);
        }
    }
}
} // namespace detail

// Welds vertices of a single clustering that lie within epsilon of each other.
inline Clustering weld_clustering(const Clustering &clustering, const double epsilon, const bool average_positions = true) {
    if (!(epsilon > 0.0) || !std::isfinite(epsilon)) {
        throw std::invalid_argument("weld_clustering: epsilon must be finite and positive");
    }

    if (clustering.is_empty()) {
        return clustering;
    }

    const std::vector<uint32_t> vertex_to_welded = detail::build_welding_map(clustering.positions, epsilon);
    const uint32_t welded_vertex_count = std::ranges::max(vertex_to_welded) + 1;

    Clustering welded = clustering;
    welded.positions = detail::build_welded_positions(clustering.positions, vertex_to_welded, welded_vertex_count, average_positions);

    for (Cluster &cluster : welded.clusters) {
        for (uint32_t &vertex_index : cluster.vertex_indices) {
            vertex_index = vertex_to_welded[vertex_index];
        }
    }

    detail::filter_degenerate_triangles(welded);

    LOG_DEBUG("Welding with {} welded and {} unique vertices", clustering.vertex_count() - welded_vertex_count, welded_vertex_count);

    validate(welded);
    return welded;
}