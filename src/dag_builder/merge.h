#pragma once

#include <span>

#include "cluster.h"

Clustering merge_clusterings(const std::span<const Clustering> clusterings, const double quantize_epsilon);
