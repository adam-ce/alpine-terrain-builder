#pragma once

#include <cstdint>
#include <vector>


struct [[nodiscard]] Partitioning {
    uint32_t partition_count = 0;
    std::vector<uint32_t> cluster_partitions; // original cluster index -> partition index
};
