#pragma once

#include <filesystem>
#include <utility>

#include "mesh/SimpleMesh.h"
#include "mesh/codec/from_extension.h"
#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "octree/storage/open_runtime.h"
#include "store/IndexedStorage.h"

namespace mesh::storage {

using Storage = store::Storage<octree::StoreTraits, mesh::Simple>;
using IndexedStorage = store::IndexedStorage<octree::StoreTraits, mesh::Simple>;
using OpenOptions = octree::storage::OpenOptions;

inline std::expected<IndexedStorage, store::OpenError<octree::Id>> open_index(
    const std::filesystem::path &path) {
    return store::open_index<octree::StoreTraits, mesh::Simple>(
        path,
        octree::storage::index_format(),
        mesh::codec::from_extension);
}

inline std::expected<Storage, store::OpenError<octree::Id>> open_folder(
    const std::filesystem::path &path,
    const bool create_index = false,
    OpenOptions options = {}) {
    return octree::storage::open_folder<mesh::Simple>(
        path,
        create_index,
        ".terrain",
        mesh::codec::from_extension,
        std::move(options));
}

inline std::expected<IndexedStorage, store::OpenError<octree::Id>> open_folder_indexed(
    const std::filesystem::path &path,
    OpenOptions options = {}) {
    return octree::storage::open_folder_indexed<mesh::Simple>(
        path,
        ".terrain",
        mesh::codec::from_extension,
        std::move(options));
}

} // namespace mesh::storage
