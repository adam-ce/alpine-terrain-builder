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
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <expected>

#include "store/Codec.h"
#include "store/Traits.h"
#include "store/path_layout.h"

namespace store {

template <HierarchyTraits Traits, typename NodeData>
class RawStorage {
public:
    using Key = typename Traits::Key;
    using value_type = NodeData;

    RawStorage(path_layout::Resolver<Key> layout, std::unique_ptr<Codec<NodeData>> codec)
        : m_layout(std::move(layout))
        , m_codec(std::move(codec))
    {
    }

    RawStorage(const RawStorage&) = delete;
    RawStorage& operator=(const RawStorage&) = delete;
    RawStorage(RawStorage&&) noexcept = default;
    RawStorage& operator=(RawStorage&&) noexcept = default;

    Expected<NodeData> load(const Key& key, const std::optional<NodeDataHashView> expected_hash = std::nullopt) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_codec->read(m_layout.node_path(key), expected_hash);
    }

    Expected<NodeData> load_part(const Key& key, const NodePart<NodeData>& part, const std::optional<NodeDataHashView> expected_hash = std::nullopt) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_codec->read_part(m_layout.node_path(key), part, expected_hash);
    }

    Expected<NodeDataHash> save(const Key& key, const NodeData& data) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_codec->write(m_layout.node_path(key), data);
    }

    bool supports_hashing() const { return m_codec->supports_hashing(); }

    Expected<std::vector<std::filesystem::path>> paths(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_codec->paths(m_layout.node_path(key));
    }

    Expected<bool> has(const Key& key) const
    {
        auto node_paths = paths(key);
        if (!node_paths) {
            return Error::propagate(std::move(node_paths));
        }
        if (node_paths->empty()) {
            return false;
        }
        for (const auto& path : node_paths.value()) {
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            if (error) {
                return Error::fail(Error::Code::Io, "check existence of", path, error);
            }
            if (!exists) {
                return false;
            }
        }
        return true;
    }

    // Hard-links the node's files from source, whose existence the caller has checked. Both
    // storages must use the same codec, and the target files must not exist.
    Expected<void> link_from(const Key& key, const RawStorage& source) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }

        const std::filesystem::path probe("__codec_probe__/node");
        if (m_codec->paths(probe) != source.m_codec->paths(probe)) {
            return Error::fail(Error::Code::Unsupported, "link node " + Traits::key_to_string(key) + " between storages with different codecs");
        }

        const auto source_paths = source.m_codec->paths(source.m_layout.node_path(key));
        const auto target_paths = m_codec->paths(m_layout.node_path(key));
        if (source_paths.size() != target_paths.size()) {
            return Error::fail(Error::Code::Internal, "matching codec probes produced different actual path counts");
        }
        for (size_t index = 0; index < source_paths.size(); ++index) {
            std::error_code error;
            std::filesystem::create_directories(target_paths[index].parent_path(), error);
            if (error) {
                return Error::fail(Error::Code::Io, "create directories", target_paths[index].parent_path(), error);
            }
            std::filesystem::create_hard_link(source_paths[index], target_paths[index], error);
            if (error) {
                return Error::fail(Error::Code::Io, "create hard link", source_paths[index], target_paths[index], error);
            }
        }
        return {};
    }

    const path_layout::Resolver<Key>& layout() const { return m_layout; }

private:
    path_layout::Resolver<Key> m_layout;
    std::unique_ptr<Codec<NodeData>> m_codec;
};

} // namespace store
