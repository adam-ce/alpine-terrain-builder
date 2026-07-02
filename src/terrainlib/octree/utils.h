#pragma once

#include <functional>
#include <vector>

#include "octree/Id.h"

namespace octree {

inline void for_each_descendant_at_level(const Id &node, const Id::Level target_level, const std::function<void(const Id &)> &callback) {
    if (target_level <= node.level()) {
        return;
    }

    const Id::Level level_diff = target_level - node.level();
    const Id::Index start = node.index_on_level() << (3 * level_diff);
    const Id::Index count = Id::Index(1) << (3 * level_diff);

    for (Id::Index i = 0; i < count; ++i) {
        callback(Id(target_level, start + i));
    }
}

inline std::vector<Id> list_descendant_at_level(const Id &node, const Id::Level target_level) {
    if (target_level <= node.level()) {
        return {};
    }

    const Id::Level level_diff = target_level - node.level();
    const Id::Index start = node.index_on_level() << (3 * level_diff);
    const Id::Index count = Id::Index(1) << (3 * level_diff);

    std::vector<Id> nodes;
    nodes.reserve(count);
    for (Id::Index i = 0; i < count; ++i) {
        nodes.emplace_back(target_level, start + i);
    }
    return nodes;
}
}
