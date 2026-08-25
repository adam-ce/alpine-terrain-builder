#pragma once

#include <filesystem>
#include <optional>
#include <utility>

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

    std::filesystem::path node_path(const Key& key) const { return m_base_path / m_mapping.key_to_node_path(key); }

    std::optional<Key> key_from_node_path(const std::filesystem::path& node_path) const
    {
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(node_path, m_base_path, error);
        if (error || relative.empty() || relative.is_absolute()) {
            return std::nullopt;
        }
        return m_mapping.node_path_to_key(relative);
    }

    const std::filesystem::path& base_path() const { return m_base_path; }
    PathMapping<Key> mapping() const { return m_mapping; }

private:
    std::filesystem::path m_base_path;
    PathMapping<Key> m_mapping;
};

} // namespace store
