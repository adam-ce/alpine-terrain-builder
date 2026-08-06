#pragma once

#include <cstdint>
#include <vector>


struct [[nodiscard]] Partitioning {
    uint32_t partition_count = 0;
    uint32_t clusters_per_partition = 0; // nominal size the partitioning targeted
    std::vector<uint32_t> cluster_partitions; // original cluster index -> partition index
};
