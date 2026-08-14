#pragma once

#include <functional>
#include <utility>

#include <libassert/assert.hpp>

#include "octree/IndexMap.h"
#include "store/traverse.h"

namespace octree {

using TraversalOrder = store::TraversalOrder;

constexpr bool always_refine(const Id &) {
    return true;
}

template<typename VisitFn, typename RefineFn = std::function<bool(const Id &)>>
void traverse(
    const IndexMap &index,
    VisitFn &&visit,
    RefineFn &&refine = always_refine,
    const Id &root = Id::root(),
    const TraversalOrder order = TraversalOrder::DepthFirst) {
    const auto result = store::traverse(
        index.shared(),
        std::forward<VisitFn>(visit),
        std::forward<RefineFn>(refine),
        root,
        order);
    DEBUG_ASSERT(result.has_value());
}

} // namespace octree
