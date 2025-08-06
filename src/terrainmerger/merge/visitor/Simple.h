#pragma once

#include "merge/Result.h"
#include "merge/NodeData.h"
#include "merge/visitor/Base.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"

namespace merge::visitor {
namespace {
inline Result merge_meshes(
    const SimpleMesh &base_mesh, // left
    const SimpleMesh &new_mesh // right
) {
    // If one mesh is empty, return the other
    if (base_mesh.is_empty()) {
        return Unchanged { false };
    }
    if (new_mesh.is_empty()) {
        return Unchanged { true };
    }

    // TODO: this doesnt really work well if there are intersections
    const SimpleMesh merged_mesh = mesh::merge(base_mesh, new_mesh);
    return Merged {merged_mesh};
}
}

class Simple : public Base {
public:
    using Status = octree::NodeStatusOrMissing;

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &,
        const NodeData<LeftStatus> &left,
        const NodeData<RightStatus> &right) {

        if constexpr (right.status() == Status::Missing) {
            return Unchanged{false};
        }

        if constexpr (left.status() == Status::Missing) {
            return Unchanged{true};
        }

        if constexpr (left.status() == Status::Leaf && right.status() == Status::Leaf) {
            return merge_meshes(right.mesh(), left.mesh());
        }

        return Recurse{};
    }
};
}