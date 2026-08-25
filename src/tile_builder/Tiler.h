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

#include "ctb/Grid.hpp"
#include "ctb/types.hpp"
#include <radix/tile.h>

class Tiler
{
public:
    Tiler(ctb::Grid grid, const radix::tile::SrsBounds& bounds, radix::tile::Border border);

    [[nodiscard]] const radix::tile::SrsBounds& bounds() const;
    void setBounds(const radix::tile::SrsBounds& newBounds);
    [[nodiscard]] radix::tile::Descriptor tile_for(const radix::tile::Id& tile_id) const;

protected:
    [[nodiscard]] const ctb::Grid& grid() const;
    [[nodiscard]] ctb::i_tile grid_size() const;
    [[nodiscard]] ctb::i_tile tile_size() const;
    [[nodiscard]] radix::tile::Border border_south_east() const;

private:

    const ctb::Grid m_grid;
    radix::tile::SrsBounds m_bounds;
    const radix::tile::Border m_border_south_east;
};
