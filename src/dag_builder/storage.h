#pragma once

#include "dag_node.h"
#include "octree/storage/Storage.h"
#include "octree/storage/IndexedStorage.h"

namespace octree {

using DagStorage = Storage_<dag::ClusterBatch>;
using IndexedDagStorage = IndexedStorage_<dag::ClusterBatch>;

}
