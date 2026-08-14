#pragma once

#include <filesystem>
#include <utility>

#include "mesh/storage.h"
#include "octree/Storage.h"

namespace octree {

using OpenOptions = mesh::storage::OpenOptions;

inline std::expected<IndexedStorage, store::OpenError<Id>> open_index(
    const std::filesystem::path &path) {
    return mesh::storage::open_index(path);
}

inline std::expected<Storage, store::OpenError<Id>> open_folder(
    const std::filesystem::path &path,
    const bool create_index = false,
    OpenOptions options = {}) {
    return mesh::storage::open_folder(path, create_index, std::move(options));
}

inline std::expected<IndexedStorage, store::OpenError<Id>> open_folder_indexed(
    const std::filesystem::path &path,
    OpenOptions options = {}) {
    return mesh::storage::open_folder_indexed(path, std::move(options));
}

} // namespace octree
