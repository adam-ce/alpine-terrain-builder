/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include "ParallelTiler.h"

#include "Exception.h"
#include <functional>

ParallelTiler::ParallelTiler(const ctb::Grid& grid, const radix::tile::SrsBounds& bounds, radix::tile::Border border, radix::tile::Scheme scheme) : Tiler(grid, bounds, border, scheme)
{
}

radix::tile::Id ParallelTiler::southWestTile(unsigned zoom_level) const {
    return grid().crsToTile(bounds().min, zoom_level).to(scheme());
}

radix::tile::Id ParallelTiler::northEastTile(unsigned zoom_level) const
{
    const auto epsilon = grid().resolution(zoom_level) / 100;
    return grid().crsToTile(bounds().max - epsilon, zoom_level).to(scheme());
}

std::vector<radix::tile::Descriptor> ParallelTiler::generateTiles(unsigned zoom_level) const
{
    // in the tms scheme south west corresponds to the smaller numbers. hence we can iterate from sw to ne
    const auto sw = southWestTile(zoom_level).to(radix::tile::Scheme::Tms).coords;
    const auto ne = northEastTile(zoom_level).to(radix::tile::Scheme::Tms).coords;

    std::vector<radix::tile::Descriptor> tiles;
    tiles.reserve((ne.y - sw.y + 1) * (ne.x - sw.x + 1));
    for (auto ty = sw.y; ty <= ne.y; ++ty) {
        for (auto tx = sw.x; tx <= ne.x; ++tx) {
            const auto tile_id = radix::tile::Id{zoom_level, {tx, ty}, radix::tile::Scheme::Tms}.to(scheme());
            tiles.emplace_back(tile_for(tile_id));
            if (tiles.size() >= 1'000'000'000)
                // think about creating an on the fly tile generator. storing so many tiles takes a lot of memory.
                throw Exception("Setting the zoom level so higher is probably not a good idea. This would generate more than 1'000 million tiles. "
                                "I'm aborting. If you really need this, then that means that the future is bright. But you'll have to edit the code..\n"
                                "      .   .     \n     / \\_/ \\    \n    | O  O |   \n    | ~V~  |  _\n     ~_ _ /  // \n    /     \\ //  \n"
                                "    |  ||  |/  \n    | /  \\ |   \n    ||    ||   \n               \n");
        }
    }

    return tiles;
}

std::vector<radix::tile::Descriptor> ParallelTiler::generateTiles(const std::pair<unsigned, unsigned> &zoom_range) const {
    std::vector<radix::tile::Descriptor> tiles;
    assert(zoom_range.first <= zoom_range.second);
    for (ctb::i_zoom i = zoom_range.first; i <= zoom_range.second; ++i) {
        auto zoom_level_tiles = generateTiles(i);
        tiles.reserve(tiles.size() + zoom_level_tiles.size());
        std::move(zoom_level_tiles.begin(), zoom_level_tiles.end(), std::back_inserter(tiles));
    }
    return tiles;
}
