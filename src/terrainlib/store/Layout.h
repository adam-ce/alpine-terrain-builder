#pragma once

#include <filesystem>
#include <optional>
#include <utility>

#include "store/NodePath.h"
#include "store/PathMapping.h"

namespace store {

template<typename Key>
class Layout {
public:
    Layout(std::filesystem::path base_path, PathMapping<Key> mapping)
        : _base_path(std::move(base_path)), _mapping(mapping) {}

    NodePath node_path(const Key &key) const {
        return NodePath(_base_path / _mapping.key_to_node_path(key).path());
    }

    std::optional<Key> key_from_node_path(const NodePath &node_path) const {
        std::error_code error;
        const std::filesystem::path relative =
            std::filesystem::relative(node_path.path(), _base_path, error);
        if (error || relative.empty() || relative.is_absolute()) {
            return std::nullopt;
        }
        return _mapping.node_path_to_key(NodePath(relative));
    }

    const std::filesystem::path &base_path() const {
        return _base_path;
    }
    PathMapping<Key> mapping() const {
        return _mapping;
    }

private:
    std::filesystem::path _base_path;
    PathMapping<Key> _mapping;
};

} // namespace store
