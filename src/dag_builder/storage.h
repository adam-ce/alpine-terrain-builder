/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <filesystem>
#include <utility>

#include "codec.h"
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

inline Expected<IndexedStorage> open_index(
    const std::filesystem::path &path) {
    return store::open_index<octree::StoreTraits, dag::ClusterBatch>(
        path,
        octree::storage::index_format(),
        payload_class,
        dag::codec::cluster_batch_from_extension);
}

inline Expected<Storage> open_folder(
    const std::filesystem::path &path,
    OpenOptions options = {}) {
    return octree::storage::open_folder<dag::ClusterBatch>(
        path,
        std::string(payload_class),
        ".dag",
        dag::codec::cluster_batch_from_extension,
        std::move(options));
}

inline Expected<IndexedStorage>
open_folder_indexed(const std::filesystem::path &path, OpenOptions options = {}) {
    return octree::storage::open_folder_indexed<dag::ClusterBatch>(
        path,
        std::string(payload_class),
        ".dag",
        dag::codec::cluster_batch_from_extension,
        std::move(options));
}

inline Expected<IndexedMetadataStorage>
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
