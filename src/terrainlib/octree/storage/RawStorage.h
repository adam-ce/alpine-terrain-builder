#pragma once

#include <filesystem>
#include <system_error>

#include <expected>

#include "octree/Id.h"
#include "octree/disk/Layout.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/CopyError.h"
#include "octree/storage/defaults.h"

namespace octree {

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
class RawStorage_ {
public:
    using value_type = T;
    using codec_type = Codec;
    using load_error = typename Codec::load_error;
    using save_error = typename Codec::save_error;

    explicit RawStorage_(disk::Layout layout) noexcept : _layout(std::move(layout)) {}

    ~RawStorage_() = default;
    RawStorage_ &operator=(const RawStorage_ &) = delete;
    RawStorage_(const RawStorage_ &) = delete;
    RawStorage_(RawStorage_ &&) = default;
    RawStorage_ &operator=(RawStorage_ &&) = default;
    
    std::expected<T, load_error> load(const Id &id) const noexcept {
        const auto path = this->path_for(id);
        return Codec::load_from_path(path);
    }

    std::expected<void, save_error> save(const Id &id, const T &node) const noexcept {
        const auto path = this->path_for(id);
        return Codec::save_to_path(node, path);
    }

    std::expected<void, CopyError> copy_to(const Id &id, RawStorage_ &target) const noexcept {
        return target.copy_from(id, *this);
    }

    std::expected<void, CopyError> copy_from(const Id &id, const RawStorage_ &source) noexcept {
        if (!source.has(id)) {
            return std::unexpected(CopyErrorKind::FileNotFound);
        }

        const auto source_path = source.path_for(id);
        const auto target_path = this->path_for(id);
        if (source_path.extension() != target_path.extension()) {
            // TODO: should this error instead?
            const auto load_result = source.load(id);
            if (!load_result.has_value()) {
                return std::unexpected(CopyErrorKind::Read);
            }
            const value_type node = load_result.value();

            const auto save_result = this->save(id, node);
            if (!save_result.has_value()) {
                return std::unexpected(CopyErrorKind::Write);
            }
            return {};
        }

        std::error_code ec;
        if (std::filesystem::remove(target_path, ec)) {
            if (ec) {
                return std::unexpected(CopyErrorKind::RemoveOld);
            }
        }

        std::filesystem::create_directories(target_path.parent_path(), ec);
        if (ec) {
            return std::unexpected(CopyErrorKind::CreateDirectories);
        }

        std::filesystem::create_hard_link(source_path, target_path, ec);
        if (ec) {
            return std::unexpected(CopyErrorKind::CreateLink);
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
