#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <expected>

#include "store/Codec.h"
#include "store/path_layout.h"
#include "store/Traits.h"

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

    std::expected<NodeData, ::Error> load(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(invalid_key_error(key));
        }
        const auto result = m_codec->read(m_layout.node_path(key));
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
        return std::move(*result);
    }

    std::expected<void, ::Error> save(const Key& key, const NodeData& data) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(invalid_key_error(key));
        }
        const auto result = m_codec->write(m_layout.node_path(key), data);
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
        return {};
    }

    std::expected<std::vector<std::filesystem::path>, ::Error> paths(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(invalid_key_error(key));
        }
        return m_codec->paths(m_layout.node_path(key));
    }

    std::expected<bool, ::Error> has(const Key& key) const
    {
        const auto node_paths = paths(key);
        if (!node_paths.has_value()) {
            return std::unexpected(node_paths.error());
        }
        if (node_paths->empty()) {
            return false;
        }
        for (const auto& path : node_paths.value()) {
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            if (error) {
                return std::unexpected(::Error::make(::Error::Code::Io, "check existence of", path, error));
            }
            if (!exists) {
                return false;
            }
        }
        return true;
    }

    std::expected<bool, ::Error> remove(const Key& key) const
    {
        const auto node_paths = paths(key);
        if (!node_paths.has_value()) {
            return std::unexpected(node_paths.error());
        }
        bool removed = false;
        for (const auto& path : node_paths.value()) {
            std::error_code error;
            removed = std::filesystem::remove(path, error) || removed;
            if (error) {
                return std::unexpected(::Error::make(::Error::Code::Io, "remove", path, error));
            }
        }
        return removed;
    }

    std::expected<void, ::Error> copy_from(const Key& key, const RawStorage& source) const
    {
        return copy_from(key, source, []() -> std::expected<void, ::Error> { return {}; });
    }

    template <typename BeforeModify>
    std::expected<void, ::Error> copy_from(const Key& key, const RawStorage& source, BeforeModify&& before_modify) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(invalid_key_error(key));
        }
        const auto source_exists = source.has(key);
        if (!source_exists.has_value()) {
            return std::unexpected(source_exists.error());
        }
        if (!source_exists.value()) {
            return std::unexpected(::Error::make(::Error::Code::NotFound, "source node " + key_to_string(key) + " is missing"));
        }

        const std::filesystem::path probe("__codec_probe__/node");
        if (m_codec->paths(probe) != source.m_codec->paths(probe)) {
            auto loaded = source.m_codec->read(source.m_layout.node_path(key));
            if (!loaded.has_value()) {
                return std::unexpected(std::move(loaded).error().with_context("read source node while copying"));
            }
            const auto prepared = before_modify();
            if (!prepared.has_value()) {
                return prepared;
            }
            auto written = m_codec->write(m_layout.node_path(key), loaded.value());
            if (!written.has_value()) {
                return std::unexpected(std::move(written).error().with_context("write target node while copying"));
            }
            return {};
        }

        const auto source_paths = source.m_codec->paths(source.m_layout.node_path(key));
        const auto target_paths = m_codec->paths(m_layout.node_path(key));
        if (source_paths.size() != target_paths.size()) {
            return std::unexpected(::Error::make(::Error::Code::Internal, "matching codec probes produced different actual path counts"));
        }
        if (source_paths == target_paths) {
            return {};
        }
        const auto prepared = before_modify();
        if (!prepared.has_value()) {
            return prepared;
        }
        for (size_t index = 0; index < source_paths.size(); ++index) {
            std::error_code error;
            std::filesystem::remove(target_paths[index], error);
            if (error) {
                return std::unexpected(::Error::make(::Error::Code::Io, "remove", target_paths[index], error));
            }
            std::filesystem::create_directories(target_paths[index].parent_path(), error);
            if (error) {
                return std::unexpected(::Error::make(::Error::Code::Io, "create directories", target_paths[index].parent_path(), error));
            }
            std::filesystem::create_hard_link(source_paths[index], target_paths[index], error);
            if (error) {
                return std::unexpected(::Error::make(
                    ::Error::Code::Io, "create hard link", source_paths[index], target_paths[index], error));
            }
        }
        return {};
    }

    const path_layout::Resolver<Key>& layout() const { return m_layout; }
    const Codec<NodeData>& codec() const { return *m_codec; }

private:
    static ::Error invalid_key_error(const Key& key)
    {
        return ::Error::make(::Error::Code::InvalidInput, "invalid hierarchy key " + key_to_string(key));
    }

    path_layout::Resolver<Key> m_layout;
    std::unique_ptr<Codec<NodeData>> m_codec;
};

} // namespace store
