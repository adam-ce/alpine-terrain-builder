#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "containers/OffsetTable.h"
#include "containers/SegmentedBuffer.h"
#include "VertexInClustering.h"
#include "containers/UnionFind.h"
#include "VecHash.h"
#include "build_config.h"
#include "cluster.h"
#include "compact.h"
#include "enumerate.h"
#include "merge/clusterings.h"
#include "mesh/connectivity/boundary.h"
#include "mesh/connectivity/triangle_compare.h"
#include "range_utils.h"
#include "spatial_lookup/Hashmap.h"
#include "validate.h"
#include "numeric/StrongDouble.h"
#include "optional_utils.h"

namespace detail {
constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

uint32_t to_flat_index(const OffsetTable &base_offsets, const VertexInClustering &vertex) {
    return base_offsets.offset(vertex.clustering_index, vertex.global_vertex_index);
}

std::vector<VertexInClustering> list_all_vertices(const std::span<const Clustering> clusterings, const OffsetTable &offsets) {
    const size_t total_vertex_count = offsets.total_size();
    std::vector<VertexInClustering> vertices;
    vertices.reserve(total_vertex_count);

    for (const auto &[clustering_index, clustering] : enumerate(clusterings)) {
        for (const auto &[global_vertex_index, position] : enumerate(clustering.positions)) {
            vertices.push_back(VertexInClustering{
                .clustering_index = static_cast<uint32_t>(clustering_index),
                .global_vertex_index = static_cast<uint32_t>(global_vertex_index)
            });
        }
    }

    return vertices;
}

std::vector<VertexInClustering> find_boundary_vertices(const std::span<const Clustering> clusterings, const OffsetTable &offsets) {
    const size_t total_vertex_count = offsets.total_size();
    std::vector<VertexInClustering> boundary_vertices;
    boundary_vertices.reserve(total_vertex_count / 2);

    std::vector<bool> boundary_vertex_mask;
    for (const auto &[clustering_index, clustering] : enumerate(clusterings)) {
        for (const Cluster &cluster : clustering.clusters) {
            mesh::build_boundary_vertex_mask(cluster.local_triangles, cluster.vertex_count(), boundary_vertex_mask);

            for (const auto [local_vertex_index, on_boundary] : enumerate(boundary_vertex_mask)) {
                if (!on_boundary) {
                    continue;
                }

                const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
                boundary_vertices.push_back(VertexInClustering{
                    .clustering_index = static_cast<uint32_t>(clustering_index),
                    .global_vertex_index = global_vertex_index});
            }
        }
    }

    return boundary_vertices;
}

struct CandidateEdge {
    uint32_t start_index;
    uint32_t end_index;
    double distance_sq;

    auto operator<=>(const CandidateEdge &other) const {
        return std::tuple(StrongDouble{this->distance_sq}, start_index, end_index) <=>
               std::tuple(StrongDouble{other.distance_sq}, other.start_index, other.end_index);
    }
    auto operator==(const CandidateEdge &other) const {
        return std::tuple(StrongDouble{this->distance_sq}, start_index, end_index) ==
               std::tuple(StrongDouble{other.distance_sq}, other.start_index, other.end_index);
    }
};

void validate_epsilon(const double epsilon) {
    if (!(epsilon >= 0.0) || !std::isfinite(epsilon)) {
        throw std::invalid_argument("merge_clusterings: epsilon must be finite and positive (or zero)");
    }
}

OffsetTable build_offset_table(const std::span<const Clustering> clusterings) {
    OffsetTable offsets(clusterings.size());

    for (const Clustering &clustering : clusterings) {
        offsets.append_length(clustering.vertex_count());
    }

    return offsets;
}

spatial_lookup::Hashmap3d<VertexInClustering> build_spatial_map(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> boundary_vertices,
    const double epsilon) {
    spatial_lookup::Hashmap3d<VertexInClustering> spatial_map(epsilon * 5);

    for (const VertexInClustering &vertex : boundary_vertices) {
        const glm::dvec3 &position = clusterings[vertex.clustering_index].positions[vertex.global_vertex_index];
        spatial_map.insert(position, vertex);
    }

    return spatial_map;
}

spatial_lookup::Hashmap3d<VertexInClustering> build_spatial_map(
    const std::span<const Clustering> clusterings,
    const double epsilon) {
    spatial_lookup::Hashmap3d<VertexInClustering> spatial_map(epsilon * 5);

    for (const auto &[clustering_index, clustering] : enumerate(clusterings)) {
        for (const auto &[global_vertex_index, position] : enumerate(clustering.positions)) {
            const VertexInClustering vertex{
                .clustering_index = static_cast<uint32_t>(clustering_index),
                .global_vertex_index = static_cast<uint32_t>(global_vertex_index)
            };
            spatial_map.insert(position, vertex);
        }
    }

    return spatial_map;
}

std::unordered_map<glm::dvec3, uint32_t, DVec3Hash> build_exact_spatial_map(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> vertices) {
    std::unordered_map<glm::dvec3, uint32_t, DVec3Hash> position_to_canonical;
    uint32_t next_index = 0;

    for (const VertexInClustering &vertex : vertices) {
        const glm::dvec3 &position = clusterings[vertex.clustering_index].positions[vertex.global_vertex_index];
        auto it = position_to_canonical.find(position);
        if (it != position_to_canonical.end()) {
            continue;
        }
        position_to_canonical[position] = next_index++;
    }

    return position_to_canonical;
}

std::vector<glm::dvec3> build_average_positions(
    const std::span<const Clustering> clusterings,
    const OffsetTable &base_offsets,
    const std::vector<uint32_t> &vertex_to_canonical
) {
    if (vertex_to_canonical.empty()) {
        return {};
    }

    const uint32_t position_count = std::ranges::max(vertex_to_canonical) + 1;
    std::vector<glm::dvec3> new_positions(position_count);
    std::vector<uint32_t> counts(position_count, 0);

    for (const auto &[clustering_index, clustering] : enumerate(clusterings)) {
        const uint32_t offset = base_offsets.get_begin(clustering_index);

        for (const auto &[global_vertex_index, position] : enumerate(clustering.positions)) {
            const uint32_t flat_index = offset + global_vertex_index;
            const uint32_t canonical_index = vertex_to_canonical[flat_index];

            new_positions[canonical_index] += position;
            counts[canonical_index]++;
        }
    }

    for (const auto &[position_index, count] : enumerate(counts)) {
        DEBUG_ASSERT(count > 0);
        new_positions[position_index] /= static_cast<double>(count);
    }

    return new_positions;
}

std::vector<glm::dvec3> build_last_writer_positions(
    const std::span<const Clustering> clusterings,
    const OffsetTable &base_offsets,
    const std::vector<uint32_t> &vertex_to_canonical
) {
    if (vertex_to_canonical.empty()) {
        return {};
    }

    const uint32_t position_count = std::ranges::max(vertex_to_canonical) + 1;
    std::vector<glm::dvec3> new_positions(position_count);

    for (const auto &[clustering_index, clustering] : enumerate(clusterings)) {
        const uint32_t offset = base_offsets.get_begin(clustering_index);

        for (const auto &[global_vertex_index, position] : enumerate(clustering.positions)) {
            const uint32_t flat_index = offset + global_vertex_index;
            const uint32_t canonical_index = vertex_to_canonical[flat_index];
            new_positions[canonical_index] = position;
        }
    }

    return new_positions;
}

void filter_degenerate_triangle(Clustering& clustering) {
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

Clustering rebuild_clustering(
    const std::span<const Clustering> clusterings,
    const OffsetTable &base_offsets,
    const std::vector<uint32_t> &vertex_to_canonical,
    const bool average_positions,
    const bool remove_degenerate_triangles) {
    Clustering merged;

    // Build new position buffer
    if (average_positions) {
        merged.positions = build_average_positions(clusterings, base_offsets, vertex_to_canonical);
    } else {
        merged.positions = build_last_writer_positions(clusterings, base_offsets, vertex_to_canonical);
    }

    std::vector<uint32_t> texture_id_map;
    for (const auto &[clustering_index, clustering] : enumerate(clusterings)) {
        // Prepare mapping from old to new texture id
        texture_id_map.clear();
        texture_id_map.reserve(clustering.textures.size());
        for (const cv::Mat &texture : clustering.textures) {
            texture_id_map.push_back(merged.textures.add(texture));
        }

        const uint32_t offset = base_offsets.get_begin(clustering_index);

        for (const Cluster &cluster : clustering.clusters) {
            Cluster new_cluster;
            new_cluster.local_triangles = cluster.local_triangles;
            new_cluster.uvs = cluster.uvs;
            new_cluster.id = cluster.id;
            new_cluster.texture_id = map(cluster.texture_id, [&](const auto &id) { return texture_id_map[id]; });
            new_cluster.absolute_error = cluster.absolute_error;

            new_cluster.vertex_indices.reserve(cluster.vertex_count());
            for (const uint32_t vertex_index : cluster.vertex_indices) {
                const uint32_t global_index = offset + vertex_index;
                new_cluster.vertex_indices.push_back(vertex_to_canonical[global_index]);
            }

            merged.clusters.push_back(std::move(new_cluster));
        }
    }

    if (remove_degenerate_triangles) {
        filter_degenerate_triangle(merged);
    }

    DEBUG_ASSERT(merged.cluster_count() == sum(clusterings, [](const Clustering &clustering) { return clustering.cluster_count(); }));
    validate(merged);
    return merged;
}

struct BestMatch {
    uint32_t flat_index;
    double distance_sq;
};

bool can_weld(const MergeOptions &options, const VertexInClustering &a, const VertexInClustering &b) {
    return options.allow_interior_merges || a.clustering_index != b.clustering_index;
}

UnionFind_<true, uint32_t, uint32_t> build_epsilon_neighbourhoods(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> boundary_vertices,
    const OffsetTable &base_offsets,
    const spatial_lookup::Hashmap3d<VertexInClustering> &spatial_map,
    const uint32_t total_vertex_count,
    const double epsilon,
    const MergeOptions &options) {
    UnionFind_<true, uint32_t, uint32_t> union_find(total_vertex_count);
    std::vector<VertexInClustering> matches;

    for (const VertexInClustering &vertex : boundary_vertices) {
        const glm::dvec3 &position = clusterings[vertex.clustering_index].positions[vertex.global_vertex_index];
        const uint32_t flat_index = to_flat_index(base_offsets, vertex);

        // Final all points within epsilon.
        matches.clear();
        spatial_map.find_all_near(position, epsilon, matches);

        for (const VertexInClustering &match : matches) {
            // Skip if from same source
            if (!can_weld(options, vertex, match)) {
                continue;
            }

            // Skip if already covered the reverse
            const uint32_t match_flat_index = to_flat_index(base_offsets, match);
            if (match_flat_index <= flat_index) {
                continue;
            }

            union_find.make_union(flat_index, match_flat_index);
        }
    }

    return union_find;
}

// Assign new canonical index for missing indices (non-boundary).
void assign_remaining_canonical_indices(std::vector<uint32_t> &vertex_to_canonical, uint32_t &next_canonical_index) {
    for (uint32_t &canonical_index : vertex_to_canonical) {
        if (canonical_index == INVALID_INDEX) {
            canonical_index = next_canonical_index;
            next_canonical_index++;
        }
    }
}
} // namespace detail

// Greedily matches vertices within epsilon ball as long as they belong to a different clustering.
std::vector<uint32_t> build_greedy_local_mapping(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> boundary_vertices,
    const OffsetTable &base_offsets,
    const spatial_lookup::Hashmap3d<VertexInClustering> &spatial_map,
    const uint32_t total_vertex_count,
    const double epsilon,
    const MergeOptions &options) {

    std::vector<uint32_t> vertex_to_canonical(total_vertex_count, detail::INVALID_INDEX);
    uint32_t next_index = 0;

    // Preallocate for next loop
    std::vector<VertexInClustering> matches;
    std::vector<std::optional<detail::BestMatch>> best_by_cluster;

    for (const VertexInClustering &vertex : boundary_vertices) {
        const glm::dvec3 &position = clusterings[vertex.clustering_index].positions[vertex.global_vertex_index];
        const uint32_t flat_index = detail::to_flat_index(base_offsets, vertex);

        uint32_t &canonical_index = vertex_to_canonical[flat_index];
        if (canonical_index != detail::INVALID_INDEX) {
            // Already mapped
            continue;
        }

        canonical_index = next_index;
        next_index++;

        // Find all points within epsilon
        matches.clear();
        spatial_map.find_all_near(position, epsilon, matches);
        DEBUG_ASSERT(!matches.empty());

        // Remove any matches that are in the same cluster as the current vertex
        std::erase_if(matches, [&](const VertexInClustering &match) {
            return !detail::can_weld(options, vertex, match);
        });

        switch (matches.size()) {
        case 0:
            // Found no other vertex nearby -> unique 
            break;

        case 1: {
            // Found a single other vertex nearby -> assign same canonical index
            const VertexInClustering &other_vertex = matches[0];
            const uint32_t other_flat_index = detail::to_flat_index(base_offsets, other_vertex);
            if (vertex_to_canonical[other_flat_index] == detail::INVALID_INDEX) {
                vertex_to_canonical[other_flat_index] = canonical_index;
            }
            break;
        }

        default: {
            // Found multiple nearby vertices -> merge with nearest per clustering
            best_by_cluster.assign(clusterings.size(), std::nullopt);

            for (const VertexInClustering &match : matches) {
                const uint32_t match_flat_index = detail::to_flat_index(base_offsets, match);
                if (vertex_to_canonical[match_flat_index] != detail::INVALID_INDEX) {
                    continue;
                }

                const glm::dvec3 &match_position = clusterings[match.clustering_index].positions[match.global_vertex_index];
                const double distance_sq = glm::length2(match_position - position);
                auto &best_opt = best_by_cluster[match.clustering_index];
                if (!best_opt.has_value() || distance_sq < best_opt->distance_sq) {
                    best_opt = detail::BestMatch{
                        .flat_index = match_flat_index,
                        .distance_sq = distance_sq};
                }
            }

            for (const auto &[cluster_index, best_opt] : enumerate(best_by_cluster)) {
                if (best_opt.has_value()) {
                    vertex_to_canonical[best_opt->flat_index] = canonical_index;
                }
            }

            break;
        }
        }
    }

    detail::assign_remaining_canonical_indices(vertex_to_canonical, next_index);

    return vertex_to_canonical;
}

// Merges vertices from different clusterings into the same output vertex if they are directly or transitively connected by epsilon-distance matches (thus can merge multiple from same clustering if there is an intermediate within epsilon distance of both).
std::vector<uint32_t> build_connected_component_mapping(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> boundary_vertices,
    const OffsetTable &base_offsets,
    const spatial_lookup::Hashmap3d<VertexInClustering> &spatial_map,
    const uint32_t total_vertex_count,
    const double epsilon,
    const MergeOptions &options) {
    UnionFind_<true, uint32_t, uint32_t> neighbourhoods = detail::build_epsilon_neighbourhoods(clusterings, boundary_vertices, base_offsets, spatial_map, total_vertex_count, epsilon, options);
    return neighbourhoods.get_set_labels();
}

// Merges vertices from different clusterings into the same output vertex if they are directly connected by epsilon-distance matches (thus never merges multiple from same clustering).
std::vector<uint32_t> build_multipartite_nearest_mapping(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> boundary_vertices,
    const OffsetTable &base_offsets,
    const spatial_lookup::Hashmap3d<VertexInClustering> &spatial_map,
    const uint32_t total_vertex_count,
    const double epsilon,
    const MergeOptions &options) {
    UnionFind_<true, uint32_t, uint32_t> neighbourhoods = detail::build_epsilon_neighbourhoods(clusterings, boundary_vertices, base_offsets, spatial_map, total_vertex_count, epsilon, options);
    const auto sets = neighbourhoods.get_sets_sparse();

    // Prepare output vector
    std::vector<uint32_t> vertex_to_canonical(total_vertex_count, detail::INVALID_INDEX);
    uint32_t next_canonical_index = 0;

    // Preallocate structures used in the loop
    // Keep in sync with the fallback check in merge_clusterings.
    ASSERT(clusterings.size() <= 64);
    std::vector<std::bitset<64>> local_source_masks;
    std::vector<VertexInClustering> local_vertices;
    UnionFind local_merges;

    std::vector<detail::CandidateEdge> edges;
    for (const auto &[repr_index, set] : enumerate(sets.segments())) {
        switch (set.size()) {
        case 0:
            // (0) Empty -> this vertex is part of another set and not its repr
            continue;
        case 1:
            // (1) This vertex is unique -> add directly
            vertex_to_canonical[set[0]] = next_canonical_index;
            next_canonical_index++;
            continue;
        case 2:
            // (2) Two vertices in the set -> already optimal and guaranteed from different sources
            vertex_to_canonical[set[0]] = next_canonical_index;
            vertex_to_canonical[set[1]] = next_canonical_index;
            next_canonical_index++;
            continue;
        }

        // Precompute indices for local vertices
        local_vertices.clear();
        local_vertices.reserve(set.size());
        for (const uint32_t flat_index : set) {
            const uint32_t clustering_index = base_offsets.locate(flat_index).segment;
            const uint32_t global_index = base_offsets.local_index(clustering_index, flat_index);
            const VertexInClustering vertex{clustering_index, global_index};
            local_vertices.push_back(vertex);
        }

        // Find candidate edges
        edges.clear();
        if (set.size() > 16) {
            LOG_WARN_BACKOFF("Merging clusterings with large epsilon ({}) encountered {} vertices within transitive epsilon neighbourhood.", epsilon, set.size());
        }

        for (uint32_t i = 0; i < set.size(); i++) {
            const VertexInClustering vertex_a = local_vertices[i];
            const glm::dvec3 position_a = clusterings[vertex_a.clustering_index].positions[vertex_a.global_vertex_index];

            for (uint32_t j = i + 1; j < set.size(); j++) {
                const VertexInClustering vertex_b = local_vertices[j];
                if (vertex_a.clustering_index == vertex_b.clustering_index) {
                    // Skip if from same source
                    continue;
                }
                const glm::dvec3 position_b = clusterings[vertex_b.clustering_index].positions[vertex_b.global_vertex_index];

                edges.push_back(detail::CandidateEdge{
                    .start_index = i,
                    .end_index = j,
                    .distance_sq = glm::distance2(position_a, position_b)
                });
            }
        }

        // Sort the edges
        std::ranges::sort(edges);

        // Find compatible local merges
        local_merges.reset(set.size());
        local_source_masks.assign(set.size(), {});
        for (const auto &[vertex_id, vertex] : enumerate(local_vertices)) {
            local_source_masks[vertex_id].set(vertex.clustering_index);
        }

        for (const detail::CandidateEdge &edge : edges) {
            const uint32_t start_repr = local_merges.find(edge.start_index);
            const uint32_t end_repr = local_merges.find(edge.end_index);

            if (start_repr == end_repr) {
                continue;
            }

            const std::bitset<64> &start_sources = local_source_masks[start_repr];
            const std::bitset<64> &end_sources = local_source_masks[end_repr];
            if ((start_sources & end_sources).any()) {
                continue;
            }

            if constexpr (IS_DEBUG_BUILD) {
                if (edge.distance_sq > epsilon * epsilon) {
                    LOG_WARN_BACKOFF("Transitively welding vertices {} apart with epsilon {}", std::sqrt(edge.distance_sq), epsilon);
                }
            }

            const uint32_t merged_repr = local_merges.make_union(start_repr, end_repr);
            const std::bitset<64> merged_sources = local_source_masks[start_repr] | local_source_masks[end_repr];
            local_source_masks[merged_repr] = merged_sources;
        }

        // Commit local merges to global merge map
        const auto local_sets = local_merges.get_sets_compact();
        for (const auto &local_set : local_sets.segments()) {
            if (local_set.size() < 2) {
                continue;
            }

            const uint32_t canonical_index = next_canonical_index;
            next_canonical_index++;

            for (const uint32_t vertex_id : local_set) {
                const VertexInClustering &vertex = local_vertices[vertex_id];
                const uint32_t flat_index = detail::to_flat_index(base_offsets, vertex);
                vertex_to_canonical[flat_index] = canonical_index;
            }
        }
    }

    detail::assign_remaining_canonical_indices(vertex_to_canonical, next_canonical_index);

    return vertex_to_canonical;
}

std::vector<uint32_t> build_exact_hash_mapping(
    const std::span<const Clustering> clusterings,
    const std::span<const VertexInClustering> vertices,
    const OffsetTable &base_offsets,
    const uint32_t total_vertex_count) {
    const auto position_to_canonical = detail::build_exact_spatial_map(clusterings, vertices);

    std::vector<uint32_t> flat_to_canonical;
    flat_to_canonical.resize(total_vertex_count, detail::INVALID_INDEX);
    for (const auto & vertex : vertices) {
        const glm::dvec3 &position = clusterings[vertex.clustering_index].positions[vertex.global_vertex_index];
        const uint32_t canonical_index = position_to_canonical.at(position);
        const uint32_t flat_index = detail::to_flat_index(base_offsets, vertex);
        flat_to_canonical[flat_index] = canonical_index;
    }

    uint32_t next_canonical_index = static_cast<uint32_t>(position_to_canonical.size());
    detail::assign_remaining_canonical_indices(flat_to_canonical, next_canonical_index);

    return flat_to_canonical;
}

Clustering merge_clusterings(const std::span<const Clustering> clusterings, const double epsilon, MergeOptions options) {
    if (clusterings.empty()) {
        return {};
    }

    detail::validate_epsilon(epsilon);

    if (clusterings.size() == 1) {
        return clusterings[0];
    }

    const uint32_t total_vertex_count = sum(clusterings, [](const auto &c) { return c.vertex_count(); });
    const OffsetTable base_offsets = detail::build_offset_table(clusterings);
    const std::vector<VertexInClustering> boundary_vertices = options.only_consider_boundary ? detail::find_boundary_vertices(clusterings, base_offsets) : detail::list_all_vertices(clusterings, base_offsets);

    // Check for invalid inputs and fallback to a safe or faster mode if necessary
    if (options.mode == MergeMode::MultipartiteNearest && options.allow_interior_merges) {
        LOG_WARN("MultipartiteNearest cannot weld within a clustering, falling back to ConnectedComponents");
        options.mode = MergeMode::ConnectedComponents;
    }
    if (options.mode == MergeMode::MultipartiteNearest && clusterings.size() > 64) {
        LOG_WARN("Cannot merge {} clusterings with MultipartiteNearest (max 64 sources), falling back to GreedyLocal", clusterings.size());
        options.mode = MergeMode::GreedyLocal;
    }
    if (options.mode != MergeMode::ExactHashBased && epsilon == 0.0) {
        LOG_WARN("Cannot merge clusterings with epsilon 0.0, falling back to ExactHashBased");
        options.mode = MergeMode::ExactHashBased;
    }
    if (options.mode == MergeMode::ExactHashBased && epsilon != 0.0) {
        LOG_WARN("ExactHashBased ignores epsilon, falling back to GreedyLocal");
        options.mode = MergeMode::GreedyLocal;
    }
    if (options.mode == MergeMode::ExactHashBased) {
        options.average_positions = false;
    }

    // Construct spatial map for fast epsilon-neighbourhood queries
    const spatial_lookup::Hashmap3d<VertexInClustering> spatial_map = [&]() {
        if (options.mode == MergeMode::ExactHashBased) {
            return spatial_lookup::Hashmap3d<VertexInClustering>(0.0);
        }
        return detail::build_spatial_map(clusterings, boundary_vertices, epsilon);
    }();

    // Build mapping from each vertex to its canonical representative
    std::vector<uint32_t> vertex_to_canonical;
    switch (options.mode) {
    case MergeMode::GreedyLocal:
        vertex_to_canonical = build_greedy_local_mapping(
            clusterings,
            boundary_vertices,
            base_offsets,
            spatial_map,
            total_vertex_count,
            epsilon,
            options);
        break;

    case MergeMode::ConnectedComponents:
        vertex_to_canonical = build_connected_component_mapping(
            clusterings,
            boundary_vertices,
            base_offsets,
            spatial_map,
            total_vertex_count,
            epsilon,
            options);
        break;

    case MergeMode::MultipartiteNearest:
        vertex_to_canonical = build_multipartite_nearest_mapping(
            clusterings,
            boundary_vertices,
            base_offsets,
            spatial_map,
            total_vertex_count,
            epsilon,
            options);
        break;

    case MergeMode::ExactHashBased:
        vertex_to_canonical = build_exact_hash_mapping(
            clusterings,
            boundary_vertices,
            base_offsets,
            total_vertex_count);
        break;
    }

    const size_t unique_count = max(vertex_to_canonical) + 1;
    const size_t shared_count = static_cast<size_t>(total_vertex_count) - unique_count;

    if (shared_count == 0) {
        LOG_WARN("Merging clusterings with epsilon {} welded no vertices", epsilon);
    }

    LOG_DEBUG("Merging with {} shared and {} unique vertices", shared_count, unique_count);

    return detail::rebuild_clustering(
        clusterings,
        base_offsets,
        vertex_to_canonical,
        options.average_positions,
        options.mode == MergeMode::ConnectedComponents || options.allow_interior_merges);
}
