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

#include "Dataset.h"
#include "ParallelTiler.h"
#include "TopDownTiler.h"
#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>

using namespace radix;

namespace {
void compare_tile_lists(const std::vector<radix::tile::Descriptor>& a_tiles, std::vector<radix::tile::Descriptor> b_tiles)
{
    CHECK(a_tiles.size() == b_tiles.size());

    for (const auto& tile : a_tiles) {
        const auto matching_tile_iter = std::find_if(b_tiles.begin(), b_tiles.end(), [&](const radix::tile::Descriptor& t) {
            return t.id == tile.id;
        });
        REQUIRE(matching_tile_iter != b_tiles.end());

        CHECK(tile.tileSize == matching_tile_iter->tileSize);
        CHECK(tile.gridSize == matching_tile_iter->gridSize);
        CHECK(tile.srsBounds == matching_tile_iter->srsBounds);
        CHECK(tile.srs_epsg == matching_tile_iter->srs_epsg);

        b_tiles.erase(matching_tile_iter);
    }
    CHECK(b_tiles.empty());
}

}

TEST_CASE("TopDownTiler")
{
    SECTION("mercator / level 0 all")
    {
        const auto grid = ctb::GlobalMercator();
        const auto tiler = TopDownTiler(grid, grid.getExtent(), radix::tile::Border::No);

        const auto tiles = tiler.generateTiles({0, { 0, 0 }});
        REQUIRE(tiles.size() == 4);
        const auto parallel_tiler = ParallelTiler(grid, grid.getExtent(), radix::tile::Border::No);
        compare_tile_lists(tiles, parallel_tiler.generateTiles(1));
    }

    SECTION("mercator / level 0 austria")
    {
        const auto grid = ctb::GlobalMercator();
        auto dataset = Dataset::open_shared_raster(ALP_TEST_DATA_DIR "/austria/at_mgi.tif").value();
        const auto bounds = dataset->bounds(grid.getSRS());
        const auto tiler = TopDownTiler(grid, bounds, radix::tile::Border::No);

        const auto tiles = tiler.generateTiles({0, { 0, 0 }});
        REQUIRE(tiles.size() == 1);
        const auto parallel_tiler = ParallelTiler(grid, bounds, radix::tile::Border::No);
        compare_tile_lists(tiles, parallel_tiler.generateTiles(1));
    }

    SECTION("geodetic / level 0 east half")
    {
        const auto grid = ctb::GlobalGeodetic();
        const auto tiler = TopDownTiler(grid, grid.getExtent(), radix::tile::Border::No);

        const auto tiles = tiler.generateTiles({0, { 1, 0 }});
        REQUIRE(tiles.size() == 4);
        const auto parallel_tiler = ParallelTiler(grid, {{0, -90}, {180, 90}}, radix::tile::Border::No);
        compare_tile_lists(tiles, parallel_tiler.generateTiles(1));
    }

    SECTION("geodetic / level 0 austria")
    {
        const auto grid = ctb::GlobalGeodetic();
        auto dataset = Dataset::open_shared_raster(ALP_TEST_DATA_DIR "/austria/at_mgi.tif").value();
        const auto bounds = dataset->bounds(grid.getSRS());
        const auto tiler = TopDownTiler(grid, bounds, radix::tile::Border::No);

        const auto tiles = tiler.generateTiles({0, { 1, 0 }});
        REQUIRE(tiles.size() == 1);
        const auto parallel_tiler = ParallelTiler(grid, bounds, radix::tile::Border::No);
        compare_tile_lists(tiles, parallel_tiler.generateTiles(1));
    }
}
