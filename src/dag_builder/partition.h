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
#include "uv/unwrap.h"
#include "vector_utils.h"
#include "atlas/Packer.h"
#include "mesh/igl/manifold.h"
#include "merge/clusters.h"
#include "Partitioning.h"


struct PartitionOptions {
    uint32_t clusters_per_partition = 4;
};

inline Partitioning create_partitioning(const Clustering &clustering, const PartitionOptions options = {}) {
    const uint32_t cluster_count = clustering.clusters.size();
    const uint32_t clusters_per_partition = options.clusters_per_partition;
    if (clusters_per_partition == 1) {
        std::vector<uint32_t> identity(cluster_count);
        std::iota(identity.begin(), identity.end(), 0);
        return Partitioning{cluster_count, clusters_per_partition, identity};
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
        clusters_per_partition,
        std::move(partition_result.cluster_partitions)};
}

inline Clustering apply_partitioning(const Clustering &clustering, const Partitioning &partitioning, const MergeOptions &merge_options = {}) {
    return merge_clusters(clustering, partitioning, merge_options);
}

inline Clustering partition(const Clustering &clustering, const PartitionOptions &options = {}, const MergeOptions &merge_options = {}) {
    const Partitioning partitioning = create_partitioning(clustering, options);
    return apply_partitioning(clustering, partitioning, merge_options);
}

