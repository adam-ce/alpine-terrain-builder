#pragma once

#include "mesh/storage.h"

namespace octree {

using MeshStorage = mesh::storage::Storage;
using IndexedMeshStorage = mesh::storage::IndexedStorage;
using Storage = MeshStorage;
using IndexedStorage = IndexedMeshStorage;
using StorageSettings = store::StorageSettings;

} // namespace octree
