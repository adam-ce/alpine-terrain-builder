#pragma once

#include "spatial_lookup/CellBased.h"
#include "spatial_lookup/HashmapStorage.h"

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
using Hashmap = CellBased<n_dims, Component, Value, HashmapStorage<n_dims, Component, Value>>;

template <typename Value>
using Hashmap2d = Hashmap<2, double, Value>;
template <typename Value>
using Hashmap3d = Hashmap<3, double, Value>;

} // namespace spatial_lookup
