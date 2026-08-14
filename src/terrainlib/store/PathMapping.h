#pragma once

#include <optional>
#include <string_view>

#include "store/NodePath.h"

namespace store {

template<typename Key>
struct PathMapping {
    std::string_view id;
    NodePath (*key_to_node_path)(const Key &);
    std::optional<Key> (*node_path_to_key)(const NodePath &);

    bool operator==(const PathMapping &) const = default;
};

} // namespace store
