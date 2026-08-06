#pragma once

#include <span>

#include "cluster.h"

enum class MergeMode {
    GreedyLocal,
    ConnectedComponents,
    MultipartiteNearest,
};

struct MergeOptions {
    MergeMode mode = MergeMode::MultipartiteNearest;
    bool only_consider_boundary = true;
    bool average_positions = true;
    bool allow_interior_merges = false;
};

// Merges clusterings into a single vertex space, welding vertices of different clusterings
// that lie within epsilon of each other. Clusters are neither added, removed nor reordered.
Clustering merge_clusterings(
    const std::span<const Clustering> clusterings,
    const double epsilon,
    MergeOptions options = {});
