#pragma once

#include "hash_utils.h"

namespace mesh::merging {

// Identifies a specific vertex within a particular mesh.
struct VertexId {
    size_t mesh_index;
    size_t vertex_index;

    auto operator<=>(const VertexId &) const = default;
};

}

template <>
struct std::hash<mesh::merging::VertexId> {
    size_t operator()(const mesh::merging::VertexId &v) const noexcept {
        return ::hash::combine(v.mesh_index, v.vertex_index);
    }
};
