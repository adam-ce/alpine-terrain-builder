/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 madam
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

#include "Tiler.h"

#include <utility>

Tiler::Tiler(ctb::Grid grid, const radix::tile::SrsBounds& bounds, radix::tile::Border border)
    : m_grid(std::move(grid))
    , m_bounds(bounds)
    , m_border_south_east(border)
{

}

const ctb::Grid& Tiler::grid() const
{
    return m_grid;
}

ctb::i_tile Tiler::grid_size() const
{
    return grid().tileSize();
}

ctb::i_tile Tiler::tile_size() const
{
    return grid_size() + unsigned(border_south_east());
}

radix::tile::Border Tiler::border_south_east() const
{
    return m_border_south_east;
}

radix::tile::Descriptor Tiler::tile_for(const radix::tile::Id& tile_id) const
{
    radix::tile::SrsBounds srs_bounds = grid().srsBounds(tile_id, border_south_east() == radix::tile::Border::Yes);
    return {tile_id, srs_bounds, grid().getEpsgCode(), grid_size(), tile_size()};
}

const radix::tile::SrsBounds& Tiler::bounds() const
{
    return m_bounds;
}

void Tiler::setBounds(const radix::tile::SrsBounds& newBounds)
{
    m_bounds = newBounds;
}
