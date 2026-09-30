/*****************************************************************************
 * AlpineMaps.org
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

#include "mesh/SimpleMesh.h"
#include "mesh/codec/from_extension.h"
#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "octree/storage/open_runtime.h"
#include "store/IndexedStorage.h"

namespace mesh::storage {

inline constexpr std::string_view payload_class = "mesh.Simple3d";

using Storage = store::Storage<octree::StoreTraits, mesh::Simple>;
using IndexedStorage = store::IndexedStorage<octree::StoreTraits, mesh::Simple>;
using OpenOptions = octree::storage::OpenOptions;

inline Expected<IndexedStorage> open_index(const std::filesystem::path& path)
{
    return store::open_index<octree::StoreTraits, mesh::Simple>(path, octree::storage::index_format(), payload_class, mesh::codec::from_extension);
}

inline Expected<Storage> open_folder(const std::filesystem::path& path, OpenOptions options = {})
{
    return octree::storage::open_folder<mesh::Simple>(path, std::string(payload_class), ".sfmesh", mesh::codec::from_extension, std::move(options));
}

inline Expected<IndexedStorage> open_folder_indexed(const std::filesystem::path& path, OpenOptions options = {})
{
    return octree::storage::open_folder_indexed<mesh::Simple>(path, std::string(payload_class), ".sfmesh", mesh::codec::from_extension, std::move(options));
}

} // namespace mesh::storage
