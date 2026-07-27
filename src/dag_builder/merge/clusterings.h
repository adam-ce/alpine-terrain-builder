#pragma once

#include <span>

#include "cluster.h"

enum class MergeMode {
    GreedyLocal,
    ConnectedComponents,
    MultipartiteNearest,
};

// Merges clusterings into a single vertex space, welding vertices of different clusterings
// that lie within epsilon of each other. Clusters are neither added, removed nor reordered.
Clustering merge_clusterings(
    const std::span<const Clustering> clusterings,
    const double epsilon,
    const MergeMode merge_mode = MergeMode::MultipartiteNearest,
    const bool average_positions = true);
