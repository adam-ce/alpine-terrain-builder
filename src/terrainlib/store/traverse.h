/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
    if (!root_status) {
        return Error::propagate(std::move(root_status), "read traversal root status for node " + Traits::key_to_string(root));
    }
    if (!root_status.value().has_value()) {
        return {};
    }

    if (order == TraversalOrder::DepthFirst) {
        std::function<Expected<void>(const Key&)> depth_first;
        depth_first = [&](const Key& current) -> Expected<void> {
            auto status = index.get(current);
            if (!status) {
                return Error::propagate(std::move(status), "read node status during depth-first traversal for " + Traits::key_to_string(current));
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
                        if (!result) {
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
        if (!status) {
            return Error::propagate(std::move(status), "read node status during breadth-first traversal for " + Traits::key_to_string(current));
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
