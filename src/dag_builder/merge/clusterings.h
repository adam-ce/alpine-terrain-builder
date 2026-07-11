#pragma once

#include <span>

#include "cluster.h"

enum class MergeMode {
    GreedyLocal,
    ConnectedComponents,
    MultipartiteNearest,
};

Clustering merge_clusterings(
    const std::span<const Clustering> clusterings,
    const double epsilon,
    const MergeMode merge_mode = MergeMode::MultipartiteNearest,
    const bool average_positions = true);
