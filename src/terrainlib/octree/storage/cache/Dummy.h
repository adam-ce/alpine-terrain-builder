#pragma once

#include "octree/StoreTraits.h"
#include "store/cache/Dummy.h"

namespace octree::cache {

template<typename NodeData>
using Dummy = store::cache::Dummy<StoreTraits, NodeData>;

} // namespace octree::cache
