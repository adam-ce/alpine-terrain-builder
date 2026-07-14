#pragma once

#include "merge/Result.h"
#include "merge/NodeData.h"
#include "merge/visitor/Visitor.h"
#include "mesh/merge.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"

namespace merge::visitor {

class Simple {
public:
    using Status = octree::NodeStatusOrMissing;
    struct Context{};
    using Result = merge::Result<Context>;

    Context make_root_context() {
        return {};
    }

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &,
        const NodeData<LeftStatus> &left,
        const NodeData<RightStatus> &right,
        const Context& ctx) {

        if constexpr (RightStatus == Status::Missing) {
            return Unchanged{Source::Left};
        }

        if constexpr (LeftStatus == Status::Missing) {
            return Unchanged{Source::Right};
        }

        if constexpr (LeftStatus == Status::Leaf && RightStatus == Status::Leaf) {
            return merge_meshes(right.mesh(), left.mesh());
        }

        return Recurse{ctx};
    }

private:
    Result merge_meshes(
        const SimpleMesh &base_mesh, // left
        const SimpleMesh &new_mesh // right
    ) {
        // If one mesh is empty, return the other
        if (base_mesh.is_empty()) {
            return Unchanged { Source::Right };
        }
        if (new_mesh.is_empty()) {
            return Unchanged { Source::Left };
        }

        // TODO: this doesnt really work well if there are intersections
        const SimpleMesh merged_mesh = mesh::merge(base_mesh, new_mesh);
        return Merged{merged_mesh};
    }
};

static_assert(Visitor<Simple>);

}
