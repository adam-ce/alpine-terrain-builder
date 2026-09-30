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

#include <span>

#include "cluster.h"

enum class MergeMode {
    GreedyLocal,
    ConnectedComponents,
    MultipartiteNearest,
    ExactHashBased,
};

struct MergeOptions {
    MergeMode mode = MergeMode::MultipartiteNearest;
    bool only_consider_boundary = true;
    bool average_positions = false;
    bool allow_interior_merges = false;
};

// Merges clusterings into a single vertex space, welding vertices of different clusterings
// that lie within epsilon of each other. Clusters are neither added, removed nor reordered.
Clustering merge_clusterings(
    const std::span<const Clustering> clusterings,
    const double epsilon,
    MergeOptions options = {});
