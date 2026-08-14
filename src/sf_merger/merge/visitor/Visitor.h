#pragma once

#include "merge/NodeData.h"
#include "merge/Result.h"
#include "octree/Id.h"
#include "store/NodeStatusOrMissing.h"

namespace merge {
template <typename T>
concept Visitor = requires(T t, const octree::Id &id) {
    // Must define a nested Context type
    typename T::Context;

    // Must provide a way to create the root context
    { t.make_root_context() } -> std::same_as<typename T::Context>;

    {
        t.template visit<store::NodeStatusOrMissing::Leaf, store::NodeStatusOrMissing::Leaf>(
            id,
            std::declval<const NodeData<store::NodeStatusOrMissing::Leaf> &>(),
            std::declval<const NodeData<store::NodeStatusOrMissing::Leaf> &>(),
            std::declval<const typename T::Context &>())
    } -> std::same_as<Result<typename T::Context>>;
};

} // namespace merge
