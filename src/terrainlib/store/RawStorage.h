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

    Expected<NodeData> load(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_codec->read(m_layout.node_path(key));
    }

    Expected<void> save(const Key& key, const NodeData& data) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_codec->write(m_layout.node_path(key), data);
    }

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

    Expected<bool> remove(const Key& key) const
    {
        auto node_paths = paths(key);
        if (!node_paths) {
            return Error::propagate(std::move(node_paths));
        }
        bool removed = false;
        for (const auto& path : node_paths.value()) {
            std::error_code error;
            removed = std::filesystem::remove(path, error) || removed;
            if (error) {
                return Error::fail(Error::Code::Io, "remove", path, error);
            }
        }
        return removed;
    }

    Expected<void> copy_from(const Key& key, const RawStorage& source) const
    {
        return copy_from(key, source, []() -> Expected<void> { return {}; });
    }

    template <typename BeforeModify>
    Expected<void> copy_from(const Key& key, const RawStorage& source, BeforeModify&& before_modify) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        auto source_exists = source.has(key);
        if (!source_exists) {
            return Error::propagate(std::move(source_exists));
        }
        if (!source_exists.value()) {
            return Error::fail(Error::Code::NotFound, "source node " + Traits::key_to_string(key) + " is missing");
        }

        const std::filesystem::path probe("__codec_probe__/node");
        if (m_codec->paths(probe) != source.m_codec->paths(probe)) {
            auto loaded = source.m_codec->read(source.m_layout.node_path(key));
            if (!loaded) {
                return Error::propagate(std::move(loaded), "read source node while copying");
            }
            const auto prepared = before_modify();
            if (!prepared) {
                return prepared;
            }
            auto written = m_codec->write(m_layout.node_path(key), loaded.value());
            if (!written) {
                return Error::propagate(std::move(written), "write target node while copying");
            }
            return {};
        }

        const auto source_paths = source.m_codec->paths(source.m_layout.node_path(key));
        const auto target_paths = m_codec->paths(m_layout.node_path(key));
        if (source_paths.size() != target_paths.size()) {
            return Error::fail(Error::Code::Internal, "matching codec probes produced different actual path counts");
        }
        if (source_paths == target_paths) {
            return {};
        }
        const auto prepared = before_modify();
        if (!prepared) {
            return prepared;
        }
        for (size_t index = 0; index < source_paths.size(); ++index) {
            std::error_code error;
            std::filesystem::remove(target_paths[index], error);
            if (error) {
                return Error::fail(Error::Code::Io, "remove", target_paths[index], error);
            }
            std::filesystem::create_directories(target_paths[index].parent_path(), error);
            if (error) {
                return Error::fail(Error::Code::Io, "create directories", target_paths[index].parent_path(), error);
            }
            std::filesystem::create_hard_link(source_paths[index], target_paths[index], error);
            if (error) {
                return Error::fail(
                    Error::Code::Io, "create hard link", source_paths[index], target_paths[index], error);
            }
        }
        return {};
    }

    const path_layout::Resolver<Key>& layout() const { return m_layout; }
    const Codec<NodeData>& codec() const { return *m_codec; }

private:
    path_layout::Resolver<Key> m_layout;
    std::unique_ptr<Codec<NodeData>> m_codec;
};

} // namespace store
