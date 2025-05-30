#pragma once

#include <functional>
#include <queue>

#include "octree/Id.h"
#include "octree/Storage.h"
#include "log.h"

namespace octree {

enum class TraversalOrder {
    DepthFirst,
    BreadthFirst
};

template <
    typename VisitFn,
    typename RefineFn = std::function<bool(const Id &)>>
void traverse(
    const IndexMap& index,
    VisitFn visit_fn,
    RefineFn refine_fn = [](const Id &) { return true; },
    const Id &root = Id::root(),
    TraversalOrder order = TraversalOrder::DepthFirst) {
    if (!index.is_present(root)) {
        return;
    }

    if (order == TraversalOrder::DepthFirst) {
        std::function<void(const Id&)> dfs;
        dfs = [&](const Id& current) {
            auto current_status_opt = index.get(current);
            if (!current_status_opt) {
                return;
            }
            const auto current_status = current_status_opt.value();
            
            visit_fn(current, current_status);

            if (current.has_children() && refine_fn(current)) {
                const auto children = current.children().value();
                for (const auto& child : children) {
                    dfs(child);
                }
            }
        };
        dfs(root);
    } else if (order == TraversalOrder::BreadthFirst) {
        std::queue<Id> queue;
        queue.push(root);

        while (!queue.empty()) {
            Id current = queue.front();
            queue.pop();

            auto current_status_opt = index.get(current);
            if (!current_status_opt) {
                return;
            }
            const auto current_status = current_status_opt.value();

            visit_fn(current, current_status);

            if (current.has_children() && refine_fn(current)) {
                const auto children = current.children().value();
                for (const auto& child : children) {
                    queue.push(child);
                }
            }
        }
    } else {
        UNREACHABLE();
    }
}


} // namespace octree
