/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek
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

#include "TopDownTiler.h"

template <typename ReadFunction, typename AggregateFunction>
auto traverse_depth_first_and_aggregate(const TopDownTiler& tiler, ReadFunction read, AggregateFunction aggregate, const radix::tile::Id& root_tile_id, unsigned max_zoom_level)
    -> decltype(read(tiler.tile_for(root_tile_id)))
{
    using DataType = decltype(read(tiler.tile_for(root_tile_id)));
    const auto subtiles = tiler.generateTiles(root_tile_id);
    // subtiles can be empty if the parent tile had an overlap with the dataset extent only on the border pixels.
    if (root_tile_id.zoom_level == max_zoom_level || subtiles.empty()) {
        return read(tiler.tile_for(root_tile_id));
    }

    std::vector<DataType> unaggregated_data;
    for (const auto& tile : subtiles) {
        auto tile_data = traverse_depth_first_and_aggregate(tiler, read, aggregate, tile.id, max_zoom_level);
        unaggregated_data.emplace_back(std::move(tile_data));
    }

    return aggregate(unaggregated_data);
}
