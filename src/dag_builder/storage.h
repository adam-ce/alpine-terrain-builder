#pragma once

#include "serialization.h"
#include "octree/storage/Storage.h"
#include "octree/storage/IndexedStorage.h"
namespace octree {

using DagStorage = Storage_<dag::ClusterBatch>;
using IndexedDagStorage = IndexedStorage_<dag::ClusterBatch>;

using DagMetaStorage = Storage_<dag::NodeMetadata>;
using IndexedDagMetaStorage = IndexedStorage_<dag::NodeMetadata>;

}
