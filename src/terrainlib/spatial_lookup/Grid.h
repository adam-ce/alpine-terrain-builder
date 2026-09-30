/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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
