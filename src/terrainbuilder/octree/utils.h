#pragma once

#include <functional>
#include <vector>

#include "octree/id.h"

namespace octree {

void for_each_descendant_at_level(const Id &node, const Level target_level, const std::function<void(const Id &)> &callback) {
    if (target_level <= node.level()) {
        return;
    }

    const Level level_diff = target_level - node.level();
    const Index start = node.index_on_level() << (3 * level_diff);
    const Index count = Index(1) << (3 * level_diff);

    for (Index i = 0; i < count; ++i) {
        callback(Id(target_level, start + i));
    }
}

std::vector<Id> list_descendant_at_level(const Id &node, const Level target_level) {
    if (target_level <= node.level()) {
        return {};
    }

    const Level level_diff = target_level - node.level();
    const Index start = node.index_on_level() << (3 * level_diff);
    const Index count = Index(1) << (3 * level_diff);

    std::vector<Id> nodes;
    nodes.reserve(count);
    for (Index i = 0; i < count; ++i) {
        nodes.emplace_back(target_level, start + i);
    }
    return nodes;
}

}
