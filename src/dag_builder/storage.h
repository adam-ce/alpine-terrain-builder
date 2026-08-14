#pragma once

#include <filesystem>
#include <utility>

#include "codec/from_extension.h"
#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "octree/storage/open_runtime.h"
#include "serialization.h"
#include "store/IndexedStorage.h"

namespace octree {

using DagStorage = store::Storage<StoreTraits, dag::ClusterBatch>;
using IndexedDagStorage = store::IndexedStorage<StoreTraits, dag::ClusterBatch>;
using DagMetaStorage = store::Storage<StoreTraits, dag::NodeMetadata>;
using IndexedDagMetaStorage = store::IndexedStorage<StoreTraits, dag::NodeMetadata>;

} // namespace octree

namespace dag::storage {

using OpenOptions = octree::storage::OpenOptions;

inline std::expected<octree::IndexedDagStorage, store::OpenError<octree::Id>> open_index(
    const std::filesystem::path &path) {
    return store::open_index<octree::StoreTraits, dag::ClusterBatch>(
        path,
        octree::storage::index_format(),
        dag::codec::from_extension);
}

inline std::expected<octree::DagStorage, store::OpenError<octree::Id>> open_folder(
    const std::filesystem::path &path,
    const bool create_index = false,
    OpenOptions options = {}) {
    return octree::storage::open_folder<dag::ClusterBatch>(
        path,
        create_index,
        ".bin",
        dag::codec::from_extension,
        std::move(options));
}

inline std::expected<octree::IndexedDagStorage, store::OpenError<octree::Id>>
open_folder_indexed(const std::filesystem::path &path, OpenOptions options = {}) {
    return octree::storage::open_folder_indexed<dag::ClusterBatch>(
        path,
        ".bin",
        dag::codec::from_extension,
        std::move(options));
}

inline std::expected<octree::IndexedDagMetaStorage, store::OpenError<octree::Id>>
open_metadata_indexed(const std::filesystem::path &path) {
    const std::filesystem::path index_path =
        path.filename() == octree::disk::v1::index_file_name()
        ? path
        : path / octree::disk::v1::index_file_name();
    return store::open_index<octree::StoreTraits, dag::NodeMetadata>(
        index_path,
        octree::storage::index_format(),
        dag::codec::metadata_from_extension);
}

} // namespace dag::storage
