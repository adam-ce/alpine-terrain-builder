#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <expected>

#include "store/Codec.h"
#include "store/Layout.h"
#include "store/StorageError.h"
#include "store/Traits.h"

namespace store {

template <HierarchyTraits Traits, typename NodeData>
class RawStorage {
public:
    using Key = typename Traits::Key;
    using value_type = NodeData;

    RawStorage(Layout<Key> layout, std::unique_ptr<Codec<NodeData>> codec)
        : m_layout(std::move(layout))
        , m_codec(std::move(codec))
    {
    }

    RawStorage(const RawStorage&) = delete;
    RawStorage& operator=(const RawStorage&) = delete;
    RawStorage(RawStorage&&) noexcept = default;
    RawStorage& operator=(RawStorage&&) noexcept = default;

    std::expected<NodeData, LoadError<Key>> load(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(LoadError<Key>(InvalidKey<Key> { key }));
        }
        const auto result = m_codec->read(m_layout.node_path(key));
        if (!result.has_value()) {
            return std::unexpected(LoadError<Key>(result.error()));
        }
        return std::move(*result);
    }

    std::expected<void, SaveError<Key>> save(const Key& key, const NodeData& data) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(SaveError<Key>(InvalidKey<Key> { key }));
        }
        const auto result = m_codec->write(m_layout.node_path(key), data);
        if (!result.has_value()) {
            return std::unexpected(SaveError<Key>(result.error()));
        }
        return {};
    }

    std::expected<std::vector<std::filesystem::path>, InvalidKey<Key>> paths(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(InvalidKey<Key> { key });
        }
        return m_codec->paths(m_layout.node_path(key));
    }

    std::expected<bool, FileOperationError<Key>> has(const Key& key) const
    {
        const auto node_paths = paths(key);
        if (!node_paths.has_value()) {
            return std::unexpected(FileOperationError<Key>(node_paths.error()));
        }
        if (node_paths->empty()) {
            return false;
        }
        for (const auto& path : node_paths.value()) {
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            if (error) {
                return std::unexpected(FileOperationError<Key>(FilesystemError {
                    path,
                    "exists",
                    error,
                }));
            }
            if (!exists) {
                return false;
            }
        }
        return true;
    }

    std::expected<bool, FileOperationError<Key>> remove(const Key& key) const
    {
        const auto node_paths = paths(key);
        if (!node_paths.has_value()) {
            return std::unexpected(FileOperationError<Key>(node_paths.error()));
        }
        bool removed = false;
        for (const auto& path : node_paths.value()) {
            std::error_code error;
            removed = std::filesystem::remove(path, error) || removed;
            if (error) {
                return std::unexpected(FileOperationError<Key>(FilesystemError {
                    path,
                    "remove",
                    error,
                }));
            }
        }
        return removed;
    }

    std::expected<void, CopyError<Key>> copy_from(const Key& key, const RawStorage& source) const
    {
        return copy_from(key, source, []() -> std::expected<void, CopyError<Key>> { return {}; });
    }

    template <typename BeforeModify>
    std::expected<void, CopyError<Key>> copy_from(const Key& key, const RawStorage& source, BeforeModify&& before_modify) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(CopyError<Key>(InvalidKey<Key> { key }));
        }
        const auto source_exists = source.has(key);
        if (!source_exists.has_value()) {
            return std::unexpected(std::visit([](const auto& error) -> CopyError<Key> { return error; }, source_exists.error()));
        }
        if (!source_exists.value()) {
            return std::unexpected(CopyError<Key>(MissingSource<Key> { key }));
        }

        const NodePath probe("__codec_probe__/node");
        if (m_codec->paths(probe) != source.m_codec->paths(probe)) {
            const auto loaded = source.m_codec->read(source.m_layout.node_path(key));
            if (!loaded.has_value()) {
                return std::unexpected(CopyError<Key>(loaded.error()));
            }
            const auto prepared = before_modify();
            if (!prepared.has_value()) {
                return prepared;
            }
            const auto written = m_codec->write(m_layout.node_path(key), loaded.value());
            if (!written.has_value()) {
                return std::unexpected(CopyError<Key>(written.error()));
            }
            return {};
        }

        const auto source_paths = source.m_codec->paths(source.m_layout.node_path(key));
        const auto target_paths = m_codec->paths(m_layout.node_path(key));
        if (source_paths.size() != target_paths.size()) {
            return std::unexpected(CopyError<Key>(CodecError {
                CodecOperation::Write,
                CodecErrorCategory::Domain,
                "matching codec probes produced different actual path counts",
            }));
        }
        const auto prepared = before_modify();
        if (!prepared.has_value()) {
            return prepared;
        }
        for (size_t index = 0; index < source_paths.size(); ++index) {
            std::error_code error;
            std::filesystem::remove(target_paths[index], error);
            if (error) {
                return std::unexpected(CopyError<Key>(FilesystemError {
                    target_paths[index],
                    "remove",
                    error,
                }));
            }
            std::filesystem::create_directories(target_paths[index].parent_path(), error);
            if (error) {
                return std::unexpected(CopyError<Key>(FilesystemError {
                    target_paths[index].parent_path(),
                    "create_directories",
                    error,
                }));
            }
            std::filesystem::create_hard_link(source_paths[index], target_paths[index], error);
            if (error) {
                return std::unexpected(CopyError<Key>(FilesystemError {
                    target_paths[index],
                    "create_hard_link",
                    error,
                }));
            }
        }
        return {};
    }

    const Layout<Key>& layout() const { return m_layout; }
    const Codec<NodeData>& codec() const { return *m_codec; }

private:
    Layout<Key> m_layout;
    std::unique_ptr<Codec<NodeData>> m_codec;
};

} // namespace store
