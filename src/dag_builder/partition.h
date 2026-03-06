#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <cstdint>
#include <numeric>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

#include "cluster.h"
#include "meshopt.h"
#include "uv.h"
#include "vector_utils.h"
#include "mesh/manifold.h"
#include "OffsetTable.h"
#include "unwrap_atlas.h"

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
class [[nodiscard]] MergeResult {
public:
    explicit MergeResult(
        Cluster cluster,
        std::unordered_map<uint32_t, std::vector<std::optional<glm::dvec2>>> inconsistent_uvs,
        OffsetTable remap_offset_table) : _cluster(std::move(cluster)), _inconsistent_uvs(std::move(inconsistent_uvs)), _remap_offset_table(remap_offset_table) {}

    Cluster &cluster() noexcept {
        return this->_cluster;
    }
    const Cluster& cluster() const noexcept {
        return this->_cluster;
    }

    bool has_consistent_uvs() const noexcept {
        return this->_inconsistent_uvs.empty();
    }
    bool has_consistent_uvs(const uint32_t vertex_index) const noexcept {
        return !this->_inconsistent_uvs.contains(vertex_index);
    }

    std::optional<uint32_t> get_source_cluster(const uint32_t vertex_index) const noexcept {
        if (this->has_consistent_uvs(vertex_index)) {
            return this->_remap_offset_table.locate(vertex_index).element;
        } else {
            return std::nullopt;
        }
    }
    
    void get_source_clusters(const uint32_t vertex_index, std::vector<uint32_t>& source_clusters) const noexcept {
        source_clusters.clear();

        const auto it = this->_inconsistent_uvs.find(vertex_index);
        if (it != this->_inconsistent_uvs.end()) {
            const std::vector<std::optional<glm::dvec2>>& uvs_per_vertex = it->second;
            for (uint32_t cluster_index = 0; cluster_index < uvs_per_vertex.size(); cluster_index++) {
                if (uvs_per_vertex[cluster_index].has_value()) {
                    source_clusters.push_back(cluster_index);
                }
            }
        } else {
            const uint32_t cluster_index = this->_remap_offset_table.locate(vertex_index).element;
            source_clusters.push_back(cluster_index);
        }
    }

    const std::optional<glm::dvec2> get_uv(const uint32_t vertex_index, const uint32_t cluster_index) const noexcept {
        const auto it = this->_inconsistent_uvs.find(vertex_index);
        if (it != this->_inconsistent_uvs.end()) {
            const std::vector<std::optional<glm::dvec2>>& uvs_per_vertex = it->second;
            return uvs_per_vertex[cluster_index];
        } else {
            return this->_cluster.uvs[vertex_index];
        }
    }

private:
    Cluster _cluster;

    // This contains the inconsistent uvs encountered during merging of duplicate vertices
    // The correspoding entries in cluster.uvs are indetermine
    // Note that even if uvs from unique vertices stem from different textures, the uvs are still all stored in cluster.uvs.
    // Accessed as inconsistent_uvs[vertex_index][cluster_index]
    std::unordered_map<uint32_t, std::vector<std::optional<glm::dvec2>>> _inconsistent_uvs;

    OffsetTable _remap_offset_table;
};

inline MergeResult merge_clusters(
    const Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    std::vector<uint32_t>& vertex_remap) {
    constexpr uint32_t no_vertex_remap = -1;
#ifndef NDEBUG
    DEBUG_ASSERT(vertex_remap.size() == clustering.vertex_count());
    for (const uint32_t vertex_index : vertex_remap) {
        DEBUG_ASSERT(vertex_index == no_vertex_remap);
    }
#endif

    const uint32_t cluster_count = cluster_indices.size();

    // Contains uvs for each source cluster per inconsistent vertex.
    std::unordered_map<uint32_t, std::vector<std::optional<glm::dvec2>>> inconsistent_uvs;

    // Source ranges for remapped vertex indices
    OffsetTable remap_offset_table;
    remap_offset_table.reserve(cluster_count);

    Cluster merged;
    for (uint32_t linear_cluster_index=0; linear_cluster_index<cluster_count; linear_cluster_index++) {
        const uint32_t cluster_index = cluster_indices[linear_cluster_index];
        const Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t vertex_count = cluster.vertex_count();

        // Create vertex remapping from original vertex indices to merged vertex indices
        for (uint32_t local_vertex_index=0; local_vertex_index<vertex_count; local_vertex_index++) {
            const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
            uint32_t &merged_vertex_index = vertex_remap[global_vertex_index];

            if (merged_vertex_index == no_vertex_remap) {
                // This vertex was not yet remapped -> assign new merged index.
                merged_vertex_index = merged.vertex_indices.size();
                merged.vertex_indices.push_back(global_vertex_index);
                if (cluster.has_uvs()) {
                    merged.uvs.push_back(cluster.uvs[local_vertex_index]);
                }
            } else {
                // This is not a new vertex -> check uv consistency
                if (cluster.has_uvs()) {
                    const glm::dvec2 &new_uv = cluster.uvs[local_vertex_index];
                    const glm::dvec2 &existing_uv = merged.uvs[merged_vertex_index];
                    if (existing_uv != new_uv) {
                        // Uvs are inconsistent -> add to inconsistent list
                        auto [it, inserted] = inconsistent_uvs.try_emplace(merged_vertex_index, cluster_count, std::nullopt);
                        std::vector<std::optional<glm::dvec2>>& uvs_per_cluster = it->second;

                        // Add first occurance of vertex
                        if (inserted) {
                            const uint32_t original_linear_cluster_index = remap_offset_table.locate(merged_vertex_index).element;
                            uvs_per_cluster[original_linear_cluster_index] = existing_uv;
                        }

                        // Add new uv
                        uvs_per_cluster[linear_cluster_index] = new_uv;
                    }
                }
            }
        }

        // Mark the source range of the current cluster
        remap_offset_table.append_length(merged.vertex_count() - remap_offset_table.total_size());

        // Remap triangles to new vertex indices
        for (const auto &triangle : cluster.local_triangles) {
            glm::uvec3 remapped;
            for (uint8_t k = 0; k < 3; k++) {
                remapped[k] = vertex_remap[cluster.vertex_indices[triangle[k]]];
                DEBUG_ASSERT(remapped[k] != no_vertex_remap);
            }
            if (!is_degenerate(remapped)) {
                merged.local_triangles.push_back(remapped);
            }
        }
    }

    // Reset vertex remap
    for (const uint32_t vertex_index : merged.vertex_indices) {
        vertex_remap[vertex_index] = no_vertex_remap;
    }

    return MergeResult(std::move(merged), std::move(inconsistent_uvs), std::move(remap_offset_table));
}
} // namespace detail

inline Clustering apply_partitioning(const Clustering &clustering, const Partitioning &partitioning) {
    const uint32_t cluster_count = clustering.cluster_count();
    const size_t partition_count = partitioning.partition_count;
    const std::vector<uint32_t>& cluster_partitions = partitioning.cluster_partitions;

    // Prepare vertex remap buffer for merging
    const uint32_t no_vertex_remap = -1;
    std::vector<uint32_t> vertex_remap(clustering.positions.size(), no_vertex_remap);

    std::vector<Cluster> partitioned_clusters;
    partitioned_clusters.reserve(cluster_count);

    std::vector<uint32_t> cluster_indices;
    for (uint32_t partition_index = 0; partition_index < partition_count; partition_index++) {
        // Collect cluster indices for this partition
        cluster_indices.clear();
        for (uint32_t i = 0; i < cluster_count; i++) {
            if (cluster_partitions[i] == partition_index) {
                cluster_indices.push_back(i);
            }
        }

        // Merge clusters
        auto result = detail::merge_clusters(clustering, cluster_indices, vertex_remap);
        partitioned_clusters.push_back(std::move(result.cluster()));
        Cluster& partition = partitioned_clusters.back();

        // Collect texture ids
        std::vector<uint32_t> texture_ids;
        texture_ids.reserve(cluster_indices.size());
        for (const uint32_t cluster_index : cluster_indices) {
            texture_ids.push_back(clustering.clusters[cluster_index].texture_id);
        }
        dedup_by_sort(texture_ids);

        // Create a new uv unwrapping for this partition if the original clusters had inconsistent uvs or different textures
        const bool uvs_consistent = result.has_consistent_uvs();
        const bool has_single_texture = texture_ids.size() == 1;
        const bool needs_unwrap = !(uvs_consistent && has_single_texture);
        if (needs_unwrap) {
            // uv unwrap requires the cluster to be manifold
            // make_manifold_inplace(partition); // only changes triangles

            const std::vector<glm::dvec3> partition_positions = collect_cluster_positions(partition, clustering.positions);
            const cv::Mat texture; // TODO = clustering.textures[partition.texture];
            const mesh::View view(partition.local_triangles, partition_positions, partition.uvs, texture);
            unwrap_atlas(partition.local_triangles, partition_positions);
        }
    }

    return Clustering{
        clustering.positions,
        std::move(partitioned_clusters),
        clustering.texture};
}

inline Clustering partition(const Clustering &clustering, const PartitionOptions &options = {}) {
    const Partitioning partitioning = create_partitioning(clustering, options);
    return apply_partitioning(clustering, partitioning);
}
