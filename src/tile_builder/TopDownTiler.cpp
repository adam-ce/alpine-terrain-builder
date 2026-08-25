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

#include "TopDownTiler.h"

TopDownTiler::TopDownTiler(const ctb::Grid& grid, const radix::tile::SrsBounds& bounds, radix::tile::Border border)
    : Tiler(grid, bounds, border)
{
}

std::vector<radix::tile::Descriptor> TopDownTiler::generateTiles(const radix::tile::Id& parent_id) const
{
    const auto tile_ids = parent_id.children();
    std::vector<radix::tile::Descriptor> tiles;
    for (const auto& tile_id : tile_ids) {
        radix::tile::Descriptor t = tile_for(tile_id);
        if (intersect(bounds(), t.srsBounds))
            tiles.push_back(std::move(t));
    }

    return tiles;
}
