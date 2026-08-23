#pragma once

#include <filesystem>
#include <optional>
#include <utility>

#include "store/NodePath.h"
#include "store/PathMapping.h"

namespace store {

template <typename Key>
class Layout {
public:
    Layout(std::filesystem::path base_path, PathMapping<Key> mapping)
        : m_base_path(std::move(base_path))
        , m_mapping(mapping)
    {
    }

    NodePath node_path(const Key& key) const { return NodePath(m_base_path / m_mapping.key_to_node_path(key).path()); }

    std::optional<Key> key_from_node_path(const NodePath& node_path) const
    {
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(node_path.path(), m_base_path, error);
        if (error || relative.empty() || relative.is_absolute()) {
            return std::nullopt;
        }
        return m_mapping.node_path_to_key(NodePath(relative));
    }

    const std::filesystem::path& base_path() const { return m_base_path; }
    PathMapping<Key> mapping() const { return m_mapping; }

private:
    std::filesystem::path m_base_path;
    PathMapping<Key> m_mapping;
};

} // namespace store
