#pragma once

#include <cstdint>

#include "hash_utils.h"

struct VertexInCluster {
    uint32_t cluster_index;
    uint32_t local_vertex_index;

    auto operator<=>(const VertexInCluster &other) const = default;
};

namespace std {
    template <>
    struct hash<VertexInCluster> {
        size_t operator()(const VertexInCluster &v) const noexcept {
            return ::hash::combine(v.cluster_index, v.local_vertex_index);
        }
    };
}
