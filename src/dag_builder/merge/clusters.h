#pragma once

#include <algorithm>
#include <bitset>
#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>
#include <optional>
#include <ranges>

#include <glm/glm.hpp>

#include "build_config.h"
#include "cluster.h"
#include "enumerate.h"
#include "log.h"
#include "mesh/connected_components.h"
#include "mesh/igl/cut_to_disk.h"
#include "mesh/igl/manifold.h"
#include "mesh/manifold.h"
#include "mesh/merging/VertexMapping.h"
#include "mesh/split.h"
#include "mesh/topology.h"
#include "opencv_utils.h"
#include "optional_utils.h"
#include "range_utils.h"
#include "uv/unwrap.h"
#include "vector_utils.h"
#include "atlas/TextureBaker.h"
#include "TinyVector.h"
#include "Partitioning.h"
#include "merge_options.h"


namespace detail {
inline bool check_all_same_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t cluster_count = cluster_indices.size();
    if (cluster_count <= 1) {
        return true;
    }

    const Cluster &first_cluster = clustering.clusters[cluster_indices[0]];
    for (uint32_t i = 1; i < cluster_count; i++) {
        const Cluster &cluster = clustering.clusters[cluster_indices[i]];
        if (cluster.has_uvs() != first_cluster.has_uvs()) {
            return false;
        }
        if (cluster.texture_id != first_cluster.texture_id) {
            return false;
        }
    }
    return true;
}

inline bool check_consistent_uvs(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t uv_count = sum(cluster_indices | std::views::transform([&](const uint32_t i) {
        return clustering.clusters[i].uvs.size();
    }));

    struct VertexEntry {
        uint32_t global_index;
        glm::dvec2 uv;
    };

    // Buffer for vertex entries
    std::vector<VertexEntry> buffer;
    buffer.reserve(uv_count);

    // Collect all cluster vertices
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        if (!cluster.has_uvs()) {
            continue;
        }

        const uint32_t vertex_count = cluster.vertex_count();
        for (uint32_t i = 0; i < vertex_count; i++) {
            buffer.push_back({cluster.vertex_indices[i], cluster.uvs[i]});
        }
    }
    DEBUG_ASSERT(buffer.size() == uv_count);

    // Sort vertices by global index
    std::sort(buffer.begin(), buffer.end(), [](const VertexEntry &a, const VertexEntry &b) {
        return a.global_index < b.global_index;
    });

    // Check for inconsistent uvs.
    for (uint32_t i = 1; i < uv_count; i++) {
        if (buffer[i].global_index == buffer[i - 1].global_index) {
            if (buffer[i].uv != buffer[i - 1].uv) {
                return false;
            }
        }
    }

    return true;
}

inline bool check_merge_needs_unwrap(const Clustering &clustering, const std::span<const uint32_t> cluster_indices, const bool allow_texture_reuse) {
    if (cluster_indices.empty()) {
        return false;
    }

    // Without a single source texture there is nothing to bake.
    const bool any_texture = std::ranges::any_of(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].has_texture();
    });
    if (!any_texture) {
        return false;
    }

    if (!allow_texture_reuse) {
        return true;
    }

    if (!check_all_same_texture(clustering, cluster_indices)) {
        return true;
    }

    if (!check_consistent_uvs(clustering, cluster_indices)) {
        return true;
    }

    return false;
}

using VertexId = mesh::merging::VertexId;

inline Cluster merge_clusters_simple(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const std::span<uint32_t> vertex_remap /* scratch buffer */
) {
    constexpr uint32_t no_vertex_remap = -1;
    if constexpr (IS_DEBUG_BUILD) {
        DEBUG_ASSERT(vertex_remap.size() == clustering.vertex_count());
        for (const uint32_t vertex_index : vertex_remap) {
            DEBUG_ASSERT(vertex_index == no_vertex_remap);
        }
        DEBUG_ASSERT(check_consistent_uvs(clustering, cluster_indices));
    }

    const uint32_t cluster_count = cluster_indices.size();
    const bool has_uvs = std::ranges::any_of(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].has_uvs();
    });

    Cluster merged;
    for (uint32_t linear_cluster_index = 0; linear_cluster_index < cluster_count; linear_cluster_index++) {
        const uint32_t cluster_index = cluster_indices[linear_cluster_index];
        const Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t vertex_count = cluster.vertex_count();

        // Create vertex remapping from original vertex indices to merged vertex indices
        for (uint32_t local_vertex_index = 0; local_vertex_index < vertex_count; local_vertex_index++) {
            const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
            uint32_t &merged_vertex_index = vertex_remap[global_vertex_index];

            if (merged_vertex_index == no_vertex_remap) {
                // This vertex was not yet remapped -> assign new merged index.
                merged_vertex_index = merged.vertex_indices.size();
                merged.vertex_indices.push_back(global_vertex_index);
                if (has_uvs) {
                    // Clusters without uvs are padded to keep the merged uvs aligned with the vertices.
                    const glm::dvec2 uv = cluster.has_uvs() ? cluster.uvs[local_vertex_index] : glm::dvec2(0);
                    merged.uvs.push_back(uv);
                }
            }
        }

        // Remap triangles to new vertex indices
        for (const auto &triangle : cluster.local_triangles) {
            glm::uvec3 remapped;
            for (uint8_t k = 0; k < 3; k++) {
                remapped[k] = vertex_remap[cluster.vertex_indices[triangle[k]]];
                DEBUG_ASSERT(remapped[k] != no_vertex_remap);
            }
            if (!mesh::is_degenerate(remapped)) {
                merged.local_triangles.push_back(remapped);
            }
        }
    }

    // Reset vertex remap
    for (const uint32_t vertex_index : merged.vertex_indices) {
        vertex_remap[vertex_index] = no_vertex_remap;
    }

    return merged;
}

inline mesh::merging::VertexMapping construct_merge_mapping(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const std::span<uint32_t> vertex_remap /* scratch buffer */,
    const bool merge_duplicates_from_same_mesh = false
) {
    constexpr uint32_t no_vertex_remap = -1;
    if constexpr (IS_DEBUG_BUILD) {
        DEBUG_ASSERT(vertex_remap.size() == clustering.vertex_count());
        for (const uint32_t vertex_index : vertex_remap) {
            DEBUG_ASSERT(vertex_index == no_vertex_remap);
        }
    }

    const uint32_t cluster_count = cluster_indices.size();

    // Initialize vertex mapping
    mesh::merging::VertexMapping mapping;
    std::vector<uint32_t> vertex_counts;
    vertex_counts.reserve(cluster_count);
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        vertex_counts.push_back(cluster.vertex_count());
    }
    mapping.init(vertex_counts);

    if (cluster_indices.empty()) {
        return mapping;
    }

    // Init extra scratch buffers
    const uint32_t max_vertex_count = *std::max_element(vertex_counts.begin(), vertex_counts.end());
    struct VertexIndex {
        uint32_t local_index;
        uint32_t global_index;
    };
    std::vector<VertexIndex> cluster_vertices; // sorted vertex indices
    cluster_vertices.reserve(max_vertex_count);
    // vertex_remap[g] stores the merged index for the first occurrence (occurrence_index == 0)
    // vertex_remap_multi[g][i] stores merged indices for occurrence_index == i+1
    std::unordered_map<uint32_t, std::vector<uint32_t>> vertex_remap_multi;

    // Create actual mapping
    uint32_t next_vertex_index = 0;
    for (uint32_t linear_cluster_index = 0; linear_cluster_index < cluster_count; linear_cluster_index++) {
        const uint32_t cluster_index = cluster_indices[linear_cluster_index];
        const Cluster &cluster = clustering.clusters[cluster_index];

        // There can be multiple local vertices referencing the same global index
        // we want to detect this to assign separate new indices, thus we first sort the index list
        cluster_vertices.clear();
        for (const auto [local_vertex_index, global_vertex_index] : enumerate(cluster.vertex_indices)) {
            cluster_vertices.emplace_back(local_vertex_index, global_vertex_index);
        }
        std::sort(cluster_vertices.begin(), cluster_vertices.end(),
                  [&](const auto &a, const auto &b) { return a.global_index < b.global_index; });
        uint32_t occurrence_index = 0;
        uint32_t last_global_vertex_index = no_vertex_remap;

        // Create vertex remapping from original vertex indices to merged vertex indices
        for (const auto [local_vertex_index, global_vertex_index] : cluster_vertices) {
            // Adjust occurrence index if duplicate index
            if (global_vertex_index == last_global_vertex_index) {
                occurrence_index++;
            } else {
                occurrence_index = 0;
            }
            last_global_vertex_index = global_vertex_index;

            uint32_t& first_merged_vertex_index = vertex_remap[global_vertex_index];
            uint32_t merged_vertex_index = first_merged_vertex_index;
            if (merged_vertex_index == no_vertex_remap) {
                // This vertex was not encountered until now
                DEBUG_ASSERT(occurrence_index == 0);
                merged_vertex_index = first_merged_vertex_index = next_vertex_index;
                next_vertex_index++;
            } else if (occurrence_index == 0) {
                // This is not a duplicate and there is already a remap set
                // -> nothing to do
            } else {
                // This is a duplicate vertex in the current cluster
                if (merge_duplicates_from_same_mesh) {
                    const uint32_t index = occurrence_index - 1;
                    std::vector<uint32_t> &new_indices = vertex_remap_multi[global_vertex_index];

                    if (index < new_indices.size()) {
                        merged_vertex_index = new_indices[index];
                    } else {
                        DEBUG_ASSERT(new_indices.size() == index);
                        merged_vertex_index = next_vertex_index;
                        new_indices.push_back(next_vertex_index);
                        next_vertex_index++;
                    }
                } else {
                    // ignore duplication and just add a new vertex
                    merged_vertex_index = next_vertex_index;
                    next_vertex_index++;
                }
            }

            const VertexId source_vertex{
                .mesh_index = linear_cluster_index,
                .vertex_index = local_vertex_index};
            mapping.add(source_vertex, merged_vertex_index);
        }
    }

    // Reset vertex_remap for the next call
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        for (const uint32_t global_vertex_index : cluster.vertex_indices) {
            vertex_remap[global_vertex_index] = no_vertex_remap;
        }
    }

    mapping.validate();
    return mapping;
}

inline Cluster merge_geometry_using_mapping(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const mesh::merging::VertexMapping &mapping) {
    DEBUG_ASSERT(mapping.mesh_count() == cluster_indices.size());

    // Preallocate merged cluster
    Cluster merged;
    if (mapping.empty()) {
        return merged;
    }

    const uint32_t unique_vertex_count = mapping.merged_vertex_count();
    merged.vertex_indices.resize(unique_vertex_count);

    uint32_t total_triangle_count = 0;
    for (const uint32_t cluster_index : cluster_indices) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        total_triangle_count += cluster.triangle_count();
    }
    merged.local_triangles.reserve(total_triangle_count);

    // Merge geometry
    const uint32_t cluster_count = cluster_indices.size();
    for (uint32_t linear_cluster_index = 0; linear_cluster_index < cluster_count; linear_cluster_index++) {
        const uint32_t cluster_index = cluster_indices[linear_cluster_index];
        const Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t vertex_count = cluster.vertex_count();

        // Merge vertices
        for (uint32_t local_vertex_index = 0; local_vertex_index < vertex_count; local_vertex_index++) {
            const uint32_t merged_local_index = mapping.map_forward(linear_cluster_index, local_vertex_index);
            const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
            merged.vertex_indices[merged_local_index] = global_vertex_index;
        }

        // Merge triangles
        for (const auto &triangle : cluster.local_triangles) {
            glm::uvec3 remapped;
            for (uint8_t k = 0; k < 3; k++) {
                DEBUG_ASSERT(triangle[k] < vertex_count);
                remapped[k] = mapping.map_forward(linear_cluster_index, triangle[k]);
            }
            if (!mesh::is_degenerate(remapped)) {
                merged.local_triangles.push_back(remapped);
            }
        }
    }

    return merged;
}

inline std::vector<uint32_t> calculate_vertex_counts(const std::span<const uint32_t> vertex_to_component, const uint32_t component_count) {
    std::vector<uint32_t> vertex_counts(component_count, 0);
    for (const auto [vertex_index, component_index] : enumerate(vertex_to_component)) {
        vertex_counts[component_index]++;
    }
    return vertex_counts;
}

std::vector<std::vector<uint32_t>> create_component_backwards_mapping(const mesh::ComponentsIndex &components_index, const std::vector<uint32_t>& forward) {
    const auto& [vertex_to_component, component_count] = components_index;

    // Allocate mapping storage
    std::vector<std::vector<uint32_t>> backward;
    backward.resize(component_count);
    const std::vector<uint32_t> vertex_counts = detail::calculate_vertex_counts(vertex_to_component, component_count);
    for (const uint32_t component_index : range(component_count)) {
        std::vector<uint32_t>& component_map = backward[component_index];
        const uint32_t component_vertex_count = vertex_counts[component_index];
        component_map.resize(component_vertex_count);
    }

    for (const auto [vertex_index, component_index] : enumerate(vertex_to_component)) {
        std::vector<uint32_t>& component_map = backward[component_index];
        const uint32_t index_in_component = forward[vertex_index];
        component_map[index_in_component] = vertex_index;
    }

    return backward;
}

struct ClusterAndBackwards {
    Cluster cluster;
    HybridIndexPairMap<uint32_t, uint32_t> backwards;
};

inline ClusterAndBackwards merge_cluster_geometry(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const std::span<uint32_t> vertex_remap
) {
    // Merge the geometry of the clusters
    mesh::merging::VertexMapping mapping = detail::construct_merge_mapping(clustering, cluster_indices, vertex_remap);
    Cluster merged_cluster = detail::merge_geometry_using_mapping(clustering, cluster_indices, mapping);

    // Keep only backward mapping
    auto [_, merged_to_original] = std::move(mapping).into_parts();
    return {merged_cluster, merged_to_original};
}

template <typename F>
inline void for_each_vertex_mapping_to(
    const uint32_t merged_vertex_index,
    const HybridIndexPairMap<uint32_t, uint32_t> &backwards,
    const uint32_t source_cluster_count,
    F &&f) {
    for (const uint32_t cluster_index : range(source_cluster_count)) {
        const auto opt = backwards.find(merged_vertex_index, cluster_index);
        if (!opt.has_value()) {
            continue;
        }
        const uint32_t source_vertex_index = opt.value();
        f(cluster_index, source_vertex_index);
    }
}

inline std::vector<uint32_t> make_cluster_unwrappable(Cluster &cluster) {
    std::vector<uint32_t> original_vertex_indices = std::move(cluster.vertex_indices);

    auto [manifold_triangles, backwards] = mesh::igl::make_manifold(cluster.local_triangles);
    cluster.local_triangles = std::move(manifold_triangles);

    mesh::cut_to_disk(
        cluster.local_triangles,
        [&](const uint32_t new_vertex_count) {
            backwards.resize(new_vertex_count);
        },
        [&](const uint32_t old_vertex_index, const uint32_t new_vertex_index) {
            backwards[new_vertex_index] = backwards[old_vertex_index];
        });

    cluster.vertex_indices.resize(backwards.size());
    for (const auto [new_vertex_index, original_vertex_index] : enumerate(backwards)) {
        cluster.vertex_indices[new_vertex_index] = original_vertex_indices[original_vertex_index];
    }

    return backwards;
}

// Algorithm used to retry a UV unwrap when the requested algorithm fails.
inline constexpr uv::Algorithm fallback_algorithm = uv::Algorithm::TutteBarycentricMapping;

// Each component is packed using component-local vertex indices, so the flat buffer cannot be
// assigned to the merged cluster directly.
inline std::vector<glm::dvec2> gather_packed_uvs(
    const PackedAtlas &atlas,
    const std::span<const TextureMapId> component_texture_ids,
    const std::span<const std::vector<uint32_t>> component_to_merged,
    const uint32_t merged_vertex_count) {
    std::vector<glm::dvec2> merged_uvs(merged_vertex_count);
    for (const auto &[component_index, local_to_merged] : enumerate(component_to_merged)) {
        const std::span<const glm::dvec2> component_uvs = atlas.uvs_for(component_texture_ids[component_index]);
        DEBUG_ASSERT(component_uvs.size() == local_to_merged.size());
        for (const auto [local_index, merged_index] : enumerate(local_to_merged)) {
            merged_uvs[merged_index] = component_uvs[local_index];
        }
    }
    return merged_uvs;
}

// A packed atlas together with the uvs addressing it, in merged-vertex order.
struct UvMap {
    PackedAtlas atlas;
    std::vector<glm::dvec2> uvs;
};

inline UvMap unwrap_merged_cluster(
    const Clustering &clustering,
    const Cluster &merged_cluster,
    HybridIndexPairMap<uint32_t, uint32_t> &merged_to_original,
    const std::span<const uint32_t> cluster_indices,
    const uv::Algorithm algorithm) {
    // Materialize cluster mesh
    const mesh::Simple merged_mesh = materialize_cluster(merged_cluster, clustering.positions);
    DEBUG_ASSERT(!merged_mesh.has_uvs());

    // Split into individual connectivity components
    const mesh::ComponentsIndex components_index = mesh::find_connected_components(merged_cluster.local_triangles, merged_cluster.vertex_count());
    auto [components, merged_to_component] = mesh::split_into_connected_components_with_map(merged_mesh, components_index);
    std::vector<std::vector<uint32_t>> component_to_merged = detail::create_component_backwards_mapping(components_index, merged_to_component);

    // Calculate source clusters per merged vertex
    std::vector<TinyVector<uint32_t>> source_clusters_per_vertex(merged_to_original.size());
    for (const auto [merged_index, linear_cluster_index, source_vertex_index] : merged_to_original.entries()) {
        source_clusters_per_vertex[merged_index].push_back(linear_cluster_index);
    }

    // Prepare atlas for new texture
    TextureBaker baker;

    std::vector<TextureMapId> component_texture_ids(components.size());

    // Preallocate
    std::vector<uint32_t> source_clusters;
    source_clusters.reserve(cluster_indices.size());

    // Clusters without a texture get a single black texel.
    const cv::Mat placeholder_texture = cv::Mat::zeros(1, 1, CV_8UC3);

    // Perform an uv unwrap for each component
    for (auto &[component_index, component] : enumerate(components)) {
        const std::vector<uint32_t> &local_to_merged = component_to_merged[component_index];

        // Find relevant source clusters
        constexpr size_t MAX_CLUSTER_COUNT = 128;
        ASSERT(cluster_indices.size() <= MAX_CLUSTER_COUNT);
        std::bitset<MAX_CLUSTER_COUNT> seen;
        for (const uint32_t merged_index : local_to_merged) {
            for (const uint32_t source_cluster : source_clusters_per_vertex[merged_index]) {
                seen.set(source_cluster);
            }
        }
        source_clusters.clear();
        for (const uint32_t linear_cluster_index : range(cluster_indices.size())) {
            if (seen.test(linear_cluster_index)) {
                source_clusters.push_back(linear_cluster_index);
            }
        }

        const bool reuse_source_map = source_clusters.size() == 1 && clustering.clusters[cluster_indices[source_clusters[0]]].has_texture();
        if (reuse_source_map) {
            // If one single cluster is relevant we dont need to perform a fresh unwrap
            const uint32_t linear_cluster_index = source_clusters[0];
            const uint32_t cluster_index = cluster_indices[linear_cluster_index];
            std::vector<glm::dvec2> uvs = transform_vector(local_to_merged, [&](const uint32_t merged_index) {
                const auto opt = merged_to_original.find(merged_index, linear_cluster_index);
                DEBUG_ASSERT(opt.has_value());
                const uint32_t original_vertex_index = opt.value();
                return clustering.clusters[cluster_index].uvs[original_vertex_index];
            });
            const cv::Mat texture = clustering.get_cluster_texture(cluster_index).value();
            component_texture_ids[component_index] = baker.add_mesh(TexturedMesh{component.triangles, TextureMap{uvs, texture}});
        } else {
            // If multiple clusters are relevant, we have to perform an unwrap.
            auto result = uv::unwrap(component, algorithm);
            if (!result.has_value() && algorithm != fallback_algorithm) {
                LOG_WARN("Failed to unwrap using requested algorithm: {}, falling back to TutteBarycentricMapping", result.error().description());
                result = uv::unwrap(component, fallback_algorithm);
            }
            if (!result.has_value()) {
                LOG_ERROR_AND_EXIT("Failed to unwrap using default: {}", result.error().description());
            }
            std::vector<glm::dvec2> uvs = result.value();

            // Create comp to add to baker
            TextureComposition comp;
            comp.target_uvs = std::move(uvs);
            comp.maps = transform_vector(source_clusters, [&](const uint32_t linear_cluster_index) {
                const uint32_t cluster_index = cluster_indices[linear_cluster_index];
                const Cluster& cluster = clustering.clusters[cluster_index];
                std::vector<glm::dvec2> uvs = cluster.uvs;
                uvs.resize(cluster.vertex_count(), glm::dvec2(0)); // pad uvs if missing
                const cv::Mat texture = clustering.get_cluster_texture(cluster_index).value_or(placeholder_texture);
                return TextureMap{std::move(uvs), texture};
            });
            comp.triangles.reserve(component.triangles.size());
            for (const auto& target_triangle : component.triangles) {
                uint32_t source_map = 0;

                const auto source_triangle_for = [&](const uint32_t linear_cluster_index) -> std::optional<glm::uvec3> {
                    glm::uvec3 source_triangle;
                    for (uint8_t k = 0; k < 3; k++) {
                        const uint32_t merged_index = local_to_merged[target_triangle[k]];
                        const auto opt = merged_to_original.find(merged_index, linear_cluster_index);
                        if (!opt) {
                            return std::nullopt;
                        }
                        source_triangle[k] = *opt;
                    }
                    return source_triangle;
                };

                std::optional<glm::uvec3> source_triangle;
                const uint32_t first_merged_index = local_to_merged[target_triangle[0]];
                for (const uint32_t linear_cluster_index : source_clusters_per_vertex[first_merged_index]) {
                    source_triangle = source_triangle_for(linear_cluster_index);
                    if (source_triangle) {
                        source_map = index_of(source_clusters, linear_cluster_index).value();
                        break;
                    }
                }
                DEBUG_ASSERT(source_triangle.has_value());

                comp.triangles.push_back(MappedTriangle{
                    .source_map = source_map,
                    .source = *source_triangle,
                    .target = target_triangle,
                });
            }
            component_texture_ids[component_index] = baker.add_composition(comp);
        }
    }

    PackedAtlas atlas = std::move(baker).pack();
    std::vector<glm::dvec2> uvs = gather_packed_uvs(
        atlas, component_texture_ids, component_to_merged, merged_cluster.vertex_count());

    return UvMap{std::move(atlas), std::move(uvs)};
}

struct UnwrappedCluster {
    Cluster cluster;
    PackedAtlas atlas;
};

inline UnwrappedCluster merge_clusters_with_unwrap(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const std::span<uint32_t> vertex_remap,
    const uv::Algorithm algorithm) {
    // Merge raw geometry
    auto [merged_cluster, merged_to_original] = merge_cluster_geometry(clustering, cluster_indices, vertex_remap);

    // Make each component into a topological disk to allow unwrap
    const auto remap = make_cluster_unwrappable(merged_cluster);
    for (const auto [new_vertex_index, old_vertex_index] : enumerate(remap)) {
        if (new_vertex_index != old_vertex_index) {
            for (const uint32_t linear_cluster_index : range(cluster_indices.size())) {
                if (const auto opt = merged_to_original.find(old_vertex_index, linear_cluster_index); opt.has_value()) {
                    const uint32_t source_vertex_index = opt.value();
                    merged_to_original.insert_or_assign(new_vertex_index, linear_cluster_index, source_vertex_index);
                }
            }
        }
    }

    // Unwrap each component and pack the atlas
    auto [atlas, uvs] = unwrap_merged_cluster(clustering, merged_cluster, merged_to_original, cluster_indices, algorithm);
    merged_cluster.uvs = std::move(uvs);

    return UnwrappedCluster{std::move(merged_cluster), std::move(atlas)};
}
}

// A merged clustering whose freshly unwrapped textures are packed but not rendered yet.
struct MergeResult {
    Clustering clustering;
    std::unordered_map<uint32_t, PackedAtlas> unbaked; // merged cluster index -> packed texture
    std::vector<uint32_t> source_triangle_counts; // per texture in clustering.textures
};

inline MergeResult merge_clusters_unbaked(const Clustering &clustering, const Partitioning &partitioning, const MergeOptions &options = {}) {
    const uint32_t cluster_count = clustering.cluster_count();
    const size_t partition_count = partitioning.partition_count;
    const std::vector<uint32_t> &cluster_partitions = partitioning.cluster_partitions;

    // Prepare vertex remap buffer for merging
    const uint32_t no_vertex_remap = -1;
    std::vector<uint32_t> vertex_remap(clustering.vertex_count(), no_vertex_remap);

    TextureSet textures;
    std::unordered_map<uint32_t, PackedAtlas> unbaked;
    std::vector<uint32_t> source_triangle_counts;
    std::vector<Cluster> partitioned_clusters;
    partitioned_clusters.reserve(partition_count);

    std::vector<uint32_t> cluster_indices;
    uint32_t unwrap_count = 0;
    uint32_t reuse_count = 0;
    for (uint32_t partition_index = 0; partition_index < partition_count; partition_index++) {
        // Collect cluster indices for this partition
        cluster_indices.clear();
        for (uint32_t i = 0; i < cluster_count; i++) {
            if (cluster_partitions[i] == partition_index) {
                cluster_indices.push_back(i);
            }
        }
        // Empty partitions would break the cluster index == partition index mapping relied on by build_lod.
        ASSERT(!cluster_indices.empty());

        // Check if we need to perform a fresh uv unwrap due to different textures or inconsistent uvs
        const bool needs_unwrap = detail::check_merge_needs_unwrap(clustering, cluster_indices, options.allow_texture_reuse);

        Cluster merged_cluster;
        if (needs_unwrap) {
            // We need to perform a fresh uv unwrap
            auto [cluster, atlas] = detail::merge_clusters_with_unwrap(
                clustering, cluster_indices, vertex_remap, options.uv_unwrap_algorithm);

            merged_cluster = std::move(cluster);
            unbaked.emplace(partition_index, std::move(atlas));
            unwrap_count++;
        } else {
            // We can perform a simple merge by just concatinating the triangles and deduplicating vertices.
            merged_cluster = detail::merge_clusters_simple(clustering, cluster_indices, vertex_remap);

            // Get texture from any source cluster
            if (const auto texture = clustering.get_cluster_texture(cluster_indices[0])) {
                const uint32_t texture_id = textures.add(texture.value());
                merged_cluster.texture_id = texture_id;
                source_triangle_counts.resize(textures.size(), 0);
                source_triangle_counts[texture_id] += merged_cluster.triangle_count();
            }
            reuse_count++;
        }

        // Carry the largest child error into the merged cluster.
        double max_child_error = 0.0;
        for (const uint32_t cluster_index : cluster_indices) {
            max_child_error = std::max(max_child_error, clustering.clusters[cluster_index].absolute_error);
        }
        merged_cluster.absolute_error = max_child_error;

        partitioned_clusters.push_back(std::move(merged_cluster));
    }

    Clustering new_clustering {
        clustering.positions,
        std::move(partitioned_clusters),
        textures};
    LOG_DEBUG("Merged {} partitions ({} unwrapped, {} reused)", unwrap_count + reuse_count, unwrap_count, reuse_count);
    validate(new_clustering);
    return MergeResult{std::move(new_clustering), std::move(unbaked), std::move(source_triangle_counts)};
}
