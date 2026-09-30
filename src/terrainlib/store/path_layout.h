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

#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

namespace store::path_layout {

template <typename Key>
struct Mapping {
    std::string_view id;
    std::filesystem::path (*key_to_node_path)(const Key&);
    std::optional<Key> (*node_path_to_key)(const std::filesystem::path&);

    bool operator==(const Mapping&) const = default;
};

template <typename Key>
class Resolver {
public:
    Resolver(std::filesystem::path base_path, Mapping<Key> mapping)
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
    Mapping<Key> mapping() const { return m_mapping; }

private:
    std::filesystem::path m_base_path;
    Mapping<Key> m_mapping;
};

} // namespace store::path_layout
