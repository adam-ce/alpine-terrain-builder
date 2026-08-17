#pragma once

#include <filesystem>
#include <utility>

#include "codec/from_extension.h"
#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "octree/storage/open_runtime.h"
#include "store/IndexedStorage.h"

namespace dag::storage {

inline constexpr std::string_view payload_class = "dag.ClusterBatch";

using OpenOptions = octree::storage::OpenOptions;
using Storage = store::Storage<octree::StoreTraits, dag::ClusterBatch>;
using IndexedStorage = store::IndexedStorage<octree::StoreTraits, dag::ClusterBatch>;
using MetadataStorage = store::Storage<octree::StoreTraits, dag::NodeMetadata>;
using IndexedMetadataStorage =
    store::IndexedStorage<octree::StoreTraits, dag::NodeMetadata>;

inline std::expected<IndexedStorage, store::OpenError<octree::Id>> open_index(
    const std::filesystem::path &path) {
    return store::open_index<octree::StoreTraits, dag::ClusterBatch>(
        path,
        octree::storage::index_format(),
        payload_class,
        dag::codec::from_extension);
}

inline std::expected<Storage, store::OpenError<octree::Id>> open_folder(
    const std::filesystem::path &path,
    OpenOptions options = {}) {
    return octree::storage::open_folder<dag::ClusterBatch>(
        path,
        std::string(payload_class),
        ".dag",
        dag::codec::from_extension,
        std::move(options));
}

inline std::expected<IndexedStorage, store::OpenError<octree::Id>>
open_folder_indexed(const std::filesystem::path &path, OpenOptions options = {}) {
    return octree::storage::open_folder_indexed<dag::ClusterBatch>(
        path,
        std::string(payload_class),
        ".dag",
        dag::codec::from_extension,
        std::move(options));
}

inline std::expected<IndexedMetadataStorage, store::OpenError<octree::Id>>
open_metadata_indexed(const std::filesystem::path &path) {
    const std::filesystem::path index_path =
        path.filename() == octree::storage::index_file_name
        ? path
        : path / octree::storage::index_file_name;
    return store::open_index<octree::StoreTraits, dag::NodeMetadata>(
        index_path,
        octree::storage::index_format(),
        payload_class,
        dag::codec::metadata_from_extension);
}

} // namespace dag::storage
