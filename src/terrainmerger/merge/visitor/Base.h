#pragma once

#include "merge/NodeData.h"
#include "merge/Result.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"

namespace merge::visitor {
class Base {
public:
    virtual ~Base() = default;

    template <octree::NodeStatusOrMissing LeftStatus, octree::NodeStatusOrMissing RightStatus>
    Result visit(
        const octree::Id &id,
        NodeData<LeftStatus> &left,
        NodeData<RightStatus> &right);
};
} // namespace merge::visitor
