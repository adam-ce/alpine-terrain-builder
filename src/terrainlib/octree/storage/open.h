#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"
#include "octree/disk/layout/strategy/Default.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/Storage.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/defaults.h"

namespace octree {

struct OpenOptions {
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy = {};
    std::optional<std::string> preferred_extension_with_dot = {};
};

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
tl::expected<IndexedStorage<T, Codec>, io::Error> open_index(const std::filesystem::path &index_path);
template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
Storage<T, Codec> open_folder(
    const std::filesystem::path &base_path,
    const bool create_index = false,
    OpenOptions options = {});
template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
IndexedStorage<T, Codec> open_folder_indexed(
    const std::filesystem::path &base_path,
    OpenOptions options = {});
}

#include "open.inl"

