#pragma once

#include <filesystem>
#include <system_error>

#include <tl/expected.hpp>

#include "octree/Id.h"
#include "octree/disk/Layout.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/CopyError.h"
#include "octree/storage/defaults.h"

namespace octree {

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
class RawStorage {
public:
    using value_type = T;
    using codec_type = Codec;
    using load_error = typename Codec::load_error;
    using save_error = typename Codec::save_error;

    explicit RawStorage(disk::Layout layout) noexcept : _layout(std::move(layout)) {}

    ~RawStorage() = default;
    RawStorage &operator=(const RawStorage &) = delete;
    RawStorage(const RawStorage &) = delete;
    RawStorage(RawStorage &&) = default;
    RawStorage &operator=(RawStorage &&) = default;
    
    tl::expected<T, load_error> load(const Id &id) const noexcept {
        const auto path = this->path_for(id);
        return Codec::load_from_path(path);
    }

    tl::expected<void, save_error> save(const Id &id, const T &node) const noexcept {
        const auto path = this->path_for(id);
        return Codec::save_to_path(node, path);
    }

    tl::expected<void, CopyError> copy_to(const Id &id, const RawStorage &target) noexcept {
        return target.copy_from(id, *this);
    }

    tl::expected<void, CopyError> copy_from(const Id &id, RawStorage &source) const noexcept {
        if (!source.has(id)) {
            return tl::unexpected(CopyErrorKind::FileNotFound);
        }

        const auto source_path = source.path_for(id);
        const auto target_path = this->path_for(id);
        if (source_path.extension() != target_path.extension()) {
            // TODO: should this error instead?
            const auto load_result = source.load(id);
            if (!load_result.has_value()) {
                return tl::unexpected(CopyErrorKind::Read);
            }
            const value_type node = load_result.value();

            const auto save_result = this->save(id, node);
            if (!save_result.has_value()) {
                return tl::unexpected(CopyErrorKind::Write);
            }
            return {};
        }

        std::error_code ec;
        if (std::filesystem::remove(target_path, ec)) {
            if (ec) {
                return tl::unexpected(CopyErrorKind::RemoveOld);
            }
        }

        std::filesystem::create_directories(target_path.parent_path(), ec);
        if (ec) {
            return tl::unexpected(CopyErrorKind::CreateDirectories);
        }

        std::filesystem::create_hard_link(source_path, target_path, ec);
        if (ec) {
            return tl::unexpected(CopyErrorKind::CreateLink);
        }

        return {};
    }

    bool remove(const Id &id) const noexcept {
        const auto path = this->path_for(id);
        std::error_code ec;
        const auto removed = std::filesystem::remove(path, ec);
        return !ec && removed;
    }

    bool has(const Id &id) const noexcept {
        const auto path = this->path_for(id);
        std::error_code ec;
        const auto exists = std::filesystem::exists(path, ec);
        return !ec && exists;
    }

    std::filesystem::path path_for(const Id &id) const noexcept {
        return this->_layout.get_node_path(id);
    }

    std::filesystem::path base_path() const noexcept {
        return this->_layout.base_path();
    }

    const disk::Layout &layout() const noexcept {
        return this->_layout;
    }

private:
    disk::Layout _layout;
};

} // namespace octree
