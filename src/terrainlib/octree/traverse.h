#pragma once

namespace octree {

enum class TraversalOrder {
    DepthFirst,
    BreadthFirst
};

template <
    typename VisitFn,
    typename RefineFn = std::function<bool(const Id &)>>
void traverse(
    const Storage &storage,
    VisitFn visit_fn,
    TraversalOrder order = TraversalOrder::DepthFirst,
    RefineFn refine_fn = [](const Id &) { return true; },
    const Id &root = Id::root()) {
    if (!storage.has_node(root)) {
        return;
    }

    if (order == TraversalOrder::DepthFirst) {
        std::function<void(const Id&)> dfs;
        dfs = [&](const Id& id) {
            if (!storage.has_node(id) || !filter_fn(id)) return;

            visit_fn(id);

            if (refine_fn(id)) {
                for (const auto& child_id : id.children()) {
                    dfs(child_id);
                }
            }
        };
        dfs(root_id);

    } else { // BreadthFirst
        std::queue<Id> queue;
        queue.push(root_id);

        while (!queue.empty()) {
            Id current = queue.front();
            queue.pop();

            if (!storage.has_node(current) || !filter_fn(current)) {
                continue;
            }

            visit_fn(current);

            if (refine_fn(current)) {
                for (const auto& child_id : current.children()) {
                    queue.push(child_id);
                }
            }
        }
    }
}

} // namespace octree
