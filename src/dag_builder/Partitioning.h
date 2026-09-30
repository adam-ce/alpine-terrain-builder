/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

#include "containers/SegmentedBuffer.h"
#include "enumerate.h"


struct [[nodiscard]] Partitioning {
    uint32_t partition_count = 0;
    uint32_t clusters_per_partition = 0; // nominal size the partitioning targeted
    std::vector<uint32_t> cluster_partitions; // original cluster index -> partition index
};

[[nodiscard]]
inline std::vector<uint32_t> count_partition_sizes(const Partitioning &partitioning) {
    std::vector<uint32_t> sizes(partitioning.partition_count, 0);
    for (const uint32_t partition_index : partitioning.cluster_partitions) {
        sizes[partition_index]++;
    }
    return sizes;
}

using PartitionToClusters = SegmentedBuffer<uint32_t, uint32_t>;

// Invert partitioning to map partition to clusters.
[[nodiscard]]
inline PartitionToClusters invert_partitioning(const Partitioning &partitioning) {
    std::vector<uint32_t> remaining = count_partition_sizes(partitioning);

    PartitionToClusters partition_to_clusters;
    partition_to_clusters.init(remaining);
    for (const auto [cluster_index, partition_index] : enumerate(partitioning.cluster_partitions)) {
        const uint32_t partition_size = partition_to_clusters.segment_size(partition_index);
        const uint32_t next_slot = partition_size - remaining[partition_index];
        remaining[partition_index]--;
        partition_to_clusters(partition_index, next_slot) = cluster_index;
    }
    return partition_to_clusters;
}
