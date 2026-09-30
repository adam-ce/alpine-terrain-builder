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
