#pragma once

#include "spatial_lookup/GridStorage.h"
#include "spatial_lookup/CellBased.h"

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
using Grid = CellBased<n_dims, Component, Value, GridStorage<n_dims, Component, Value>>;

template <typename Value>
using Grid2d = Grid<2, double, Value>;
template <typename Value>
using Grid3d = Grid<3, double, Value>;

}
