#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <unordered_map>
#include <vector>
#include <ranges>

#include <glm/glm.hpp>

#include "OffsetTable.h"
#include "cluster.h"
#include "enumerate.h"
#include "glm_utils.h"
#include "mesh/boundary.h"
#include "mesh/connected_components.h"
#include "mesh/igl/cut_to_disk.h"
#include "mesh/igl/orient.h"
#include "mesh/manifold.h"
#include "mesh/merging/VertexMapping.h"
#include "mesh/split.h"
#include "mesh/texture_trim.h"
#include "mesh/topology.h"
#include "meshopt.h"
#include "opencv_utils.h"
#include "range_utils.h"
#include "unwrap_atlas.h"
#include "reproject_texture.h"
#include "uv/unwrap.h"
#include "vector_utils.h"

struct PartitionOptions {
    uint32_t clusters_per_partition = 4;
};

struct [[nodiscard]] Partitioning {
    uint32_t partition_count = 0;
    std::vector<uint32_t> cluster_partitions; // original cluster index -> partition index
};

inline Partitioning create_partitioning(const Clustering &clustering, const PartitionOptions options = {}) {
    const uint32_t cluster_count = clustering.clusters.size();
    const uint32_t clusters_per_partition = options.clusters_per_partition;
    if (clusters_per_partition == 1) {
        std::vector<uint32_t> identity(cluster_count);
        std::iota(identity.begin(), identity.end(), 0);
        return Partitioning{cluster_count, identity};
    }

    // meshopt only supports float positions
    const std::vector<glm::vec3> positions_f = to_approximate_normalized(clustering.positions);

    // Prepare vertex counts per cluster and total vertex count
    std::vector<uint32_t> cluster_vertex_counts(cluster_count);
    uint32_t total_index_count = 0;
    for (uint32_t i = 0; i < cluster_count; i++) {
        cluster_vertex_counts[i] = clustering.clusters[i].vertex_indices.size();
        total_index_count += cluster_vertex_counts[i];
    }

    // Flatten vertices into index buffer
    std::vector<uint32_t> cluster_vertices(total_index_count);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < cluster_count; i++) {
        const auto &vertices = clustering.clusters[i].vertex_indices;
        std::copy(vertices.begin(), vertices.end(), cluster_vertices.begin() + offset);
        offset += vertices.size();
    }

    // Partition clusters into partitions using the helper
    meshopt::PartitionClustersResult partition_result = meshopt::partition_clusters(
        cluster_vertices,
        cluster_vertex_counts,
        positions_f,
        clusters_per_partition);

    return Partitioning{
        partition_result.partition_count,
        std::move(partition_result.cluster_partitions)};
}

namespace detail {
inline bool check_all_same_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const uint32_t cluster_count = cluster_indices.size();
    if (cluster_count <= 1) {
        return true;
    }

    const uint32_t first_texture_id = clustering.clusters[cluster_indices[0]].texture_id;
    for (uint32_t i = 1; i < cluster_count; i++) {
        if (clustering.clusters[cluster_indices[i]].texture_id != first_texture_id) {
            return false;
        }
    }
    return true;
}
inline bool check_consistent_uvs(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    const size_t uv_count = sum(cluster_indices | std::views::transform([&](const uint32_t i) {
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
inline bool check_merge_needs_unwrap(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    if (cluster_indices.empty() || clustering.textures.empty()) {
        return false;
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
    const std::span<uint32_t> vertex_remap) {
    constexpr uint32_t no_vertex_remap = -1;
#ifndef NDEBUG
    DEBUG_ASSERT(vertex_remap.size() == clustering.vertex_count());
    for (const uint32_t vertex_index : vertex_remap) {
        DEBUG_ASSERT(vertex_index == no_vertex_remap);
    }
    DEBUG_ASSERT(check_consistent_uvs(clustering, cluster_indices));
#endif

    const uint32_t cluster_count = cluster_indices.size();

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
                if (cluster.has_uvs()) {
                    merged.uvs.push_back(cluster.uvs[local_vertex_index]);
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

inline mesh::merging::VertexMapping construct_merge_mapping(const Clustering &clustering, const std::span<const uint32_t> cluster_indices, const std::span<uint32_t> vertex_remap) {
    constexpr uint32_t no_vertex_remap = -1;
#ifndef NDEBUG
    DEBUG_ASSERT(vertex_remap.size() == clustering.vertex_count());
    for (const uint32_t vertex_index : vertex_remap) {
        DEBUG_ASSERT(vertex_index == no_vertex_remap);
    }
#endif

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
        uint32_t occurance_index = 0;
        uint32_t last_global_vertex_index = no_vertex_remap;

        // Create vertex remapping from original vertex indices to merged vertex indices
        for (const auto [local_vertex_index, global_vertex_index] : cluster_vertices) {
            // Adjust occurance index if duplicate index
            if (global_vertex_index == last_global_vertex_index) {
                occurance_index++;
            } else {
                occurance_index = 0;
            }
            last_global_vertex_index = global_vertex_index;

            uint32_t& first_merged_vertex_index = vertex_remap[global_vertex_index];
            uint32_t merged_vertex_index = first_merged_vertex_index;
            if (merged_vertex_index == no_vertex_remap) {
                // This vertex was not encountered until now
                DEBUG_ASSERT(occurance_index == 0);
                merged_vertex_index = first_merged_vertex_index = next_vertex_index;
                next_vertex_index++;
            } else if (occurance_index == 0) {
                // This is not a duplicate and there is already a remap set
                // -> nothing to do
            } else {
                // This is a duplicate vertex in the current cluster
                const uint32_t index = occurance_index - 1;
                std::vector<uint32_t> &new_indices = vertex_remap_multi[global_vertex_index];

                if (index < new_indices.size()) {
                    merged_vertex_index = new_indices[index];
                } else {
                    DEBUG_ASSERT(new_indices.size() == index);
                    merged_vertex_index = next_vertex_index;
                    new_indices.push_back(next_vertex_index);
                    next_vertex_index++;
                }
            }

            const VertexId source_vertex{.mesh_index = linear_cluster_index, .vertex_index = local_vertex_index};
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

inline std::vector<uint32_t> calculate_vertex_counts(const std::vector<uint32_t> vertex_to_component, const uint32_t component_count) {
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

struct ClusterAndTexture {
    Cluster cluster;
    Texture texture;
};

inline ClusterAndTexture merge_clusters_with_unwrap(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const std::span<uint32_t> vertex_remap
) {
    // Merge the geometry of the clusters
    mesh::merging::VertexMapping mapping = detail::construct_merge_mapping(clustering, cluster_indices, vertex_remap);
    Cluster merged_cluster = detail::merge_geometry_using_mapping(clustering, cluster_indices, mapping);

    // Keep only backward mapping, since we cant keep the forward mapping valid
    auto [_, merged_to_original] = std::move(mapping).into_parts();

    // TODO: use an index mesh here

    // Make merged geometry manifold, keeping backward mapping consistent
    auto add_duplicate_vertex_to_mapping = [&](const uint32_t old_vertex_index, const uint32_t new_vertex_index) {
        for (const auto &[linear_cluster_index, cluster_index] : enumerate(cluster_indices)) {
            const mesh::merging::VertexId merged_vertex(linear_cluster_index, old_vertex_index);
            const auto it = merged_to_original.find(merged_vertex);
            if (it == merged_to_original.end()) {
                continue;
            }
            const uint32_t source_vertex_index = it->second;
            const mesh::merging::VertexId duplicated_vertex{static_cast<uint32_t>(linear_cluster_index), new_vertex_index};
            merged_to_original[duplicated_vertex] = source_vertex_index;
        }
    };
    auto duplicate_vertex = [&](const uint32_t old_vertex_index) {
        const uint32_t new_vertex_index = merged_cluster.vertex_indices.size();

        // Add new vertex to merged cluster
        merged_cluster.vertex_indices.push_back(merged_cluster.vertex_indices[old_vertex_index]);

        // Update backwards mapping
        add_duplicate_vertex_to_mapping(old_vertex_index, new_vertex_index);

        return new_vertex_index;
    };

    const auto [manifold_triangles, backwards] = mesh::igl::make_manifold(merged_cluster.local_triangles);
    std::vector<uint32_t> manifold_vertex_indices(backwards.size());
    for (const auto [i, original_index] : enumerate(backwards)) {
        manifold_vertex_indices[i] = merged_cluster.vertex_indices[original_index];
        add_duplicate_vertex_to_mapping(original_index, i);
    }
    merged_cluster.local_triangles = manifold_triangles;
    merged_cluster.vertex_indices = manifold_vertex_indices;

    /*
    mesh::make_manifold(merged_cluster.local_triangles, merged_cluster.vertex_count(), duplicate_vertex);

    const mesh::Simple merged_mesh2 = materialize_cluster(merged_cluster, clustering.positions);
    mesh::io::save_to_path(merged_mesh2, "/home/user/master/meshes/pre.glb");

    // Ensure merged geometry is consistently oriented
    mesh::orient_triangles_inplace(merged_cluster.local_triangles);

    const mesh::Simple merged_mesh3 = materialize_cluster(merged_cluster, clustering.positions);
    mesh::io::save_to_path(merged_mesh3, "/home/user/master/meshes/post.glb");
*/
    // Ensure each connectivity component is open and of genus 1 (topological disk)
    mesh::cut_to_disk(
        merged_cluster.local_triangles,
        [&](const uint32_t new_vertex_count) {
            merged_cluster.vertex_indices.resize(new_vertex_count);
        },
        [&](const uint32_t old_vertex_index, const uint32_t new_vertex_index) {
            merged_cluster.vertex_indices[new_vertex_index] = merged_cluster.vertex_indices[old_vertex_index];
            add_duplicate_vertex_to_mapping(old_vertex_index, new_vertex_index);
        });

    // Materialize cluster mesh
    const mesh::Simple merged_mesh = materialize_cluster(merged_cluster, clustering.positions);
    DEBUG_ASSERT(!merged_mesh.has_uvs());

    // Split into individual connectivity components
    const mesh::ComponentsIndex components_index = mesh::find_connected_components(merged_cluster.local_triangles, merged_cluster.vertex_count());
    auto [components, merged_to_component] = mesh::split_into_connected_components_with_map(merged_mesh, components_index);
    std::vector<std::vector<uint32_t>> component_to_merged = detail::create_component_backwards_mapping(components_index, merged_to_component);

    // Prepare atlas for new texture
    const glm::uvec2 target_texture_size(2048);
    const atlas::Plan atlas_plan = create_atlas_plan(target_texture_size, components);

    // Perform an uv unwrap for each component
    for (auto &[component_index, component] : enumerate(components)) {
        const std::vector<uint32_t> &local_to_merged = component_to_merged[component_index];

        // Find relevant source clusters
        const uint32_t original_cluster_count = cluster_indices.size();
        std::vector<uint32_t> source_clusters;
        source_clusters.reserve(original_cluster_count);
        for (const uint32_t linear_cluster_index : range(original_cluster_count)) {
            const bool cluster_is_relevant = std::ranges::any_of(local_to_merged, [&](const uint32_t merged_index) {
                const mesh::merging::VertexId merged_vertex{linear_cluster_index, merged_index};
                return merged_to_original.contains(merged_vertex);
            });

            if (cluster_is_relevant) {
                source_clusters.push_back(linear_cluster_index);
            }
        }

        const glm::uvec2 component_texture_size = atlas_plan.slots[component_index].size;
        if (source_clusters.size() == 1) {
            // If one a single cluster is relevant we dont need to perform a fresh unwrap
            const uint32_t linear_cluster_index = source_clusters[0];
            const uint32_t cluster_index = cluster_indices[linear_cluster_index];
            component.uvs = transform_vector(local_to_merged, [&](const uint32_t merged_index) {
                const mesh::merging::VertexId merged_vertex{linear_cluster_index, merged_index};
                const auto it = merged_to_original.find(merged_vertex);
                DEBUG_ASSERT(it != merged_to_original.end());
                const uint32_t original_vertex_index = it->second;
                return clustering.clusters[cluster_index].uvs[original_vertex_index];
            });
            const uint32_t texture_id = clustering.clusters[cluster_index].texture_id;
            component.texture = clustering.textures[texture_id];
            trim_texture_inplace(component);
            rescale_texture_inplace(component.texture.value(), component_texture_size);
        } else {
            // If multiple clusters are relevant, we have to perform an unwrap.
            auto result = uv::unwrap(component);
            if (!result.has_value()) {
                LOG_WARN("Failed to unwrap using ARAP: {}", result.error().description());
                result = uv::unwrap(component);
            }
            if (!result.has_value()) {
                LOG_ERROR_AND_EXIT("Failed to unwrap using default: {}", result.error().description());
            }
            component.uvs = result.value();

            // Build new texture
            auto map_to_original_triangle = [&](const uint32_t linear_cluster_index, const glm::uvec3 &triangle)
                -> std::optional<glm::uvec3> {
                glm::uvec3 original_triangle;
                for (uint8_t k = 0; k < 3; k++) {
                    const uint32_t merged_index = local_to_merged[triangle[k]];
                    const mesh::merging::VertexId merged_vertex{linear_cluster_index, merged_index};

                    const auto it = merged_to_original.find(merged_vertex);
                    if (it == merged_to_original.end()) {
                        return std::nullopt;
                    }

                    original_triangle[k] = it->second;
                }
                return original_triangle;
            };

            TextureReprojector component_texture(component_texture_size, CV_8UC3);
            for (const auto [linear_cluster_index, cluster_index] : enumerate(cluster_indices)) {
                const Cluster &cluster = clustering.clusters[cluster_index];
                const cv::Mat &cluster_texture = clustering.get_cluster_texture(cluster_index);

                for (const glm::uvec3 &triangle : component.triangles) {
                    auto opt = map_to_original_triangle(linear_cluster_index, triangle);
                    if (!opt.has_value()) {
                        continue;
                    }
                    const glm::uvec3 &original_triangle = opt.value();
                    component_texture.add_triangle(
                        cluster_texture,
                        original_triangle, cluster.uvs,
                        triangle, component.uvs);
                }
            }

            component.texture = component_texture.finish();
        }
    }

    // Assemble texture atlas
    const std::vector<cv::Mat> component_textures = transform_vector(components, [](const auto &component) { return component.texture.value(); });
    const cv::Mat merged_texture = atlas::create(atlas_plan, component_textures);
    for (auto &[component_index, component] : enumerate(components)) {
        atlas::map_uvs(atlas_plan, component_index, component.uvs);
    }

    // Finalize uvs and texture
    merged_cluster.uvs.resize(merged_cluster.vertex_count());
    const auto &vertex_to_component = components_index.vertex_to_component;
    for (const auto [merged_vertex_index, local_vertex_index] : enumerate(merged_to_component)) {
        const uint32_t component_index = vertex_to_component[merged_vertex_index];
        const mesh::Simple &component = components[component_index];
        merged_cluster.uvs[merged_vertex_index] = component.uvs[local_vertex_index];
    }

    return ClusterAndTexture{merged_cluster, merged_texture};
}
}

inline Clustering apply_partitioning(const Clustering &clustering, const Partitioning &partitioning) {
    const uint32_t cluster_count = clustering.cluster_count();
    const size_t partition_count = partitioning.partition_count;
    const std::vector<uint32_t> &cluster_partitions = partitioning.cluster_partitions;

    // Prepare vertex remap buffer for merging
    const uint32_t no_vertex_remap = -1;
    std::vector<uint32_t> vertex_remap(clustering.vertex_count(), no_vertex_remap);

    // Prepare texture remap buffer for merging
    const uint32_t no_texture_remap = -1;
    std::vector<uint32_t> texture_remap(clustering.textures.size(), no_texture_remap);
    std::vector<cv::Mat> textures;

    std::vector<Cluster> partitioned_clusters;
    partitioned_clusters.reserve(partition_count);

    std::vector<uint32_t> cluster_indices;
    for (uint32_t partition_index = 0; partition_index < partition_count; partition_index++) {
        // Collect cluster indices for this partition
        cluster_indices.clear();
        for (uint32_t i = 0; i < cluster_count; i++) {
            if (cluster_partitions[i] == partition_index) {
                cluster_indices.push_back(i);
            }
        }

        // Check if we need to perform a fresh uv unwrap due to different textures or inconsistent uvs
        const bool needs_unwrap = detail::check_merge_needs_unwrap(clustering, cluster_indices);

        Cluster merged_cluster;
        if (needs_unwrap) {
            // We need to perform a fresh uv unwrap and generate a new texture
            const auto result = detail::merge_clusters_with_unwrap(clustering, cluster_indices, vertex_remap);
            merged_cluster = result.cluster;
            merged_cluster.texture_id = textures.size();
            textures.push_back(result.texture);
        } else {
            // We can perform a simple merge by just concatinating the triangles and deduplicating vertices.
            merged_cluster = detail::merge_clusters_simple(clustering, cluster_indices, vertex_remap);

            // Add texture to new clustering, ensuring no duplicates.
            const uint32_t original_texture_id = clustering.clusters[cluster_indices[0]].texture_id;
            uint32_t& new_texture_id = texture_remap[original_texture_id];
            if (new_texture_id == no_texture_remap) {
                new_texture_id = textures.size();
                const cv::Mat &texture = clustering.textures[original_texture_id];
                textures.push_back(texture);
            }
            merged_cluster.texture_id = new_texture_id;
        }
        partitioned_clusters.push_back(std::move(merged_cluster));
    }

    const Clustering new_clustering {
        clustering.positions,
        std::move(partitioned_clusters),
        textures};
    validate(new_clustering);
    return new_clustering;
}

inline Clustering partition(const Clustering &clustering, const PartitionOptions &options = {}) {
    const Partitioning partitioning = create_partitioning(clustering, options);
    return apply_partitioning(clustering, partitioning);
}

