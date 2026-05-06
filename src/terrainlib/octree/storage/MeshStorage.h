#pragma once

#include "mesh/SimpleMesh.h"
#include "octree/storage/Storage.h"
#include "octree/storage/IndexedStorage.h"

namespace octree {

using MeshStorage = Storage<mesh::Simple>;
using IndexedMeshStorage = IndexedStorage<mesh::Simple>;

}
