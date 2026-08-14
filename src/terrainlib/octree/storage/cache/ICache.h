#pragma once

#include "octree/StoreTraits.h"
#include "store/cache/Interface.h"

namespace octree::cache {

template<typename NodeData>
using ICache = store::cache::Interface<StoreTraits, NodeData>;

} // namespace octree::cache
