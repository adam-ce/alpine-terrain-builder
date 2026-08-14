#pragma once

#include "octree/StoreTraits.h"
#include "store/cache/Lru.h"

namespace octree::cache {

template<typename NodeData>
using Lru = store::cache::Lru<StoreTraits, NodeData>;

} // namespace octree::cache
