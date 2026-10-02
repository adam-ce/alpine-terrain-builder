/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include "partition.h"
#include "priorities.h"
#include "raster_store/storage.h"
#include "selection.h"
#include "statistics.h"
#include <variant>

namespace rf_merger::produce {

using Key = radix::tile::Id;

// Largest supported zoom gap between a supplier and an output leaf. Inputs are
// validated against it.
inline constexpr unsigned max_zoom_levels = 30;

template <typename PixelType>
struct Input {
    const raster_store::storage::IndexedStorage<PixelType>* storage;
    const raster_store::io::manifest::Metadata* metadata;
};

template <typename PixelType>
struct Inputs {
    Input<PixelType> left;
    Input<PixelType> right;
    const priorities::Ranks* ranks;
};

// Hard-link the complete input tile at the leaf key.
struct Link {
    selection::Side side;
};

template <typename PixelType>
struct Created {
    raster_store::Tile<PixelType> tile;
    statistics::Category category;
};

template <typename PixelType>
using Produced = std::variant<Link, Created<PixelType>>;

// Reads original suppliers covering the leaf at the same or a coarser zoom,
// aligns them to the leaf grid and applies the selection policy. Only reads
// input storages, so workers may call it concurrently. Throws Error::Exception
// only for failed reads; the value mapping and zoom gaps must already be validated.
template <typename PixelType>
Produced<PixelType> produce(const Inputs<PixelType>& inputs, const partition::Leaf& leaf);

} // namespace rf_merger::produce
