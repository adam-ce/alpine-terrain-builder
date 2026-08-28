#pragma once

#include <expected>
#include <functional>
#include <queue>
#include <utility>

#include "store/Index.h"

namespace store {

enum class TraversalOrder {
    DepthFirst,
    BreadthFirst,
};

struct AlwaysRefine {
    template <typename Key>
    constexpr bool operator()(const Key&) const
    {
        return true;
    }
};

template <HierarchyTraits Traits, typename VisitFn, typename RefineFn = AlwaysRefine>
Expected<void> traverse(const Index<Traits>& index,
    VisitFn&& visit,
    RefineFn&& refine = {},
    const typename Traits::Key& root = Traits::root(),
    const TraversalOrder order = TraversalOrder::DepthFirst)
{
    using Key = typename Traits::Key;
    if (!Traits::is_valid(root)) {
        return store::invalid_key_error<Traits>(root);
    }
    auto root_status = index.get(root);
    if (!root_status.has_value()) {
        return Error::propagate(std::move(root_status));
    }
    if (!root_status.value().has_value()) {
        return {};
    }

    if (order == TraversalOrder::DepthFirst) {
        std::function<Expected<void>(const Key&)> depth_first;
        depth_first = [&](const Key& current) -> Expected<void> {
            auto status = index.get(current);
            if (!status.has_value()) {
                return Error::propagate(std::move(status));
            }
            if (!status.value().has_value()) {
                return {};
            }
            visit(current, status.value().value());

            if (refine(current)) {
                const auto children = Traits::children(current);
                if (children.has_value()) {
                    for (const Key& child : children.value()) {
                        auto result = depth_first(child);
                        if (!result.has_value()) {
                            return result;
                        }
                    }
                }
            }
            return {};
        };
        return depth_first(root);
    }

    std::queue<Key> queue;
    queue.push(root);
    while (!queue.empty()) {
        const Key current = queue.front();
        queue.pop();

        auto status = index.get(current);
        if (!status.has_value()) {
            return Error::propagate(std::move(status));
        }
        if (!status.value().has_value()) {
            continue;
        }
        visit(current, status.value().value());

        if (refine(current)) {
            const auto children = Traits::children(current);
            if (children.has_value()) {
                for (const Key& child : children.value()) {
                    queue.push(child);
                }
            }
        }
    }
    return {};
}

} // namespace store
