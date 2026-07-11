#pragma once

#include <cstdint>

#include "hash_utils.h"

struct VertexInClustering {
    uint32_t clustering_index;
    uint32_t global_vertex_index;

    auto operator<=>(const VertexInClustering &other) const = default;
};

namespace std {
template <>
struct hash<VertexInClustering> {
    size_t operator()(const VertexInClustering &v) const noexcept {
        return ::hash::combine(v.clustering_index, v.global_vertex_index);
    }
};
} // namespace std
