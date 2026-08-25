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

#include "Dataset.h"
#include "ParallelTiler.h"
#include "../catch2_helpers.h"
#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>
#include <glm/detail/qualifier.hpp>
#include <numeric>

using Catch::Approx;
using namespace radix;

TEST_CASE("ParallelTiler")
{
    //  const auto bounds = radix::tile::SrsBounds(1'000'000, 6'000'000, 2'000'000, 6'700'000); // in m
    SECTION("mercator / level 0")
    {
        const auto grid = ctb::GlobalMercator();
        const auto tiler = ParallelTiler(grid, grid.getExtent(), radix::tile::Border::No);

        CHECK(tiler.northEastTile(0).coords == glm::uvec2(0, 0));
        CHECK(tiler.southWestTile(0).coords == glm::uvec2(0, 0));

        const auto l0_tiles = tiler.generateTiles(0);
        REQUIRE(l0_tiles.size() == 1);
        const auto t = l0_tiles.front();
        CHECK(t.id.zoom_level == 0);
        CHECK(t.id.coords == glm::uvec2(0, 0));
        CHECK(t.gridSize == 256);
        CHECK(t.tileSize == 256);

        // all extents are equal, because we have only one tile and no border
        CHECK(t.srsBounds.min.x == Approx(grid.getExtent().min.x));
        CHECK(t.srsBounds.max.y == Approx(grid.getExtent().max.y));
        CHECK(t.srsBounds.min.y == Approx(grid.getExtent().min.y));
        CHECK(t.srsBounds.max.x == Approx(grid.getExtent().max.x));
    }

    SECTION("mercator / level 1 and 2")
    {
        const auto grid = ctb::GlobalMercator();
        auto dataset = Dataset::open_shared_raster(ALP_TEST_DATA_DIR "/austria/at_mgi.tif").value();
        const auto tiler = ParallelTiler(grid, dataset->bounds(grid.getSRS()), radix::tile::Border::No);

        CHECK(tiler.northEastTile(1).coords == glm::uvec2(1, 0));
        CHECK(tiler.southWestTile(1).coords == glm::uvec2(1, 0));

        CHECK(tiler.northEastTile(2).coords == glm::uvec2(2, 1));
        CHECK(tiler.southWestTile(2).coords == glm::uvec2(2, 1));

        {
            // https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#1/-5.80/62.29
            const auto l1_tiles = tiler.generateTiles(1);
            REQUIRE(l1_tiles.size() == 1);
            const auto t = l1_tiles.front();
            CHECK(t.id.zoom_level == 1);
            CHECK(t.id.coords == glm::uvec2(1, 0));
            CHECK(t.gridSize == 256);
            CHECK(t.tileSize == 256);

            CHECK(t.srsBounds.min.x == Approx(0));
            CHECK(t.srsBounds.min.y == Approx(0));
            CHECK(t.srsBounds.max.y == Approx(grid.getExtent().max.y));
            CHECK(t.srsBounds.max.x == Approx(grid.getExtent().max.x));
        }
        {
            const auto l2_tiles = tiler.generateTiles(2);
            REQUIRE(l2_tiles.size() == 1);
            const auto t = l2_tiles.front();
            CHECK(t.id.zoom_level == 2);
            CHECK(t.id.coords == glm::uvec2(2, 1));
            CHECK(t.gridSize == 256);
            CHECK(t.tileSize == 256);

            CHECK(t.srsBounds.min.x == Approx(0));
            CHECK(t.srsBounds.min.y == Approx(0));
            CHECK(t.srsBounds.max.y == Approx(grid.getExtent().max.y / 2));
            CHECK(t.srsBounds.max.x == Approx(grid.getExtent().max.x / 2));
        }
    }

    SECTION("geodetic / level 0")
    {
        const auto grid = ctb::GlobalGeodetic(64);
        const auto tiler = ParallelTiler(grid, grid.getExtent(), radix::tile::Border::Yes);

        CHECK(tiler.northEastTile(0).coords == glm::uvec2(1, 0));
        CHECK(tiler.southWestTile(0).coords == glm::uvec2(0, 0));

        const auto l0_tiles = tiler.generateTiles(0);
        REQUIRE(l0_tiles.size() == 2);
        const auto t0 = l0_tiles[0];
        CHECK(t0.id.zoom_level == 0);
        CHECK(t0.id.coords == glm::uvec2(0, 0));
        CHECK(t0.gridSize == 64);
        CHECK(t0.tileSize == 65);
        // north, south and west are equal to the extents (no overflow)
        CHECK(t0.srsBounds.min.y == Approx(grid.getExtent().min.y));
        CHECK(t0.srsBounds.max.y == Approx(grid.getExtent().max.y + grid.getExtent().height() / 64));
        CHECK(t0.srsBounds.min.x == Approx(grid.getExtent().min.x));
        // east should be somewher close to the 0 meridian, 1 pixel east of it to be exact)
        CHECK(t0.srsBounds.max.x == Approx(180.0 / 64));

        const auto t1 = l0_tiles[1];
        CHECK(t1.id.zoom_level == 0);
        CHECK(t1.id.coords == glm::uvec2(1, 0));
        CHECK(t1.gridSize == 64);
        CHECK(t1.tileSize == 65);
        // north and south are equal to the extent (no overflow)
        CHECK(t1.srsBounds.min.y == Approx(grid.getExtent().min.y));
        CHECK(t1.srsBounds.max.y == Approx(grid.getExtent().max.y + grid.getExtent().height() / 64));
        // west is the 0 meriaidan
        CHECK(t1.srsBounds.min.x == Approx(0));
        // east should be wrapped around. but lets make things easier for now, and clamp it.
        // it goes mostly through the pacific anyways, and I wonder whether gdal even supports wrapping around the back..
        CHECK(t1.srsBounds.max.x == Approx(grid.getExtent().max.x + grid.resolution(0)));
    }

    SECTION("geodetic / level 1 and 2")
    {
        const auto grid = ctb::GlobalGeodetic(64);
        auto dataset = Dataset::open_shared_raster(ALP_TEST_DATA_DIR "/austria/at_mgi.tif").value();
        const auto tiler = ParallelTiler(grid, dataset->bounds(grid.getSRS()), radix::tile::Border::Yes);

        CHECK(tiler.northEastTile(1).coords == glm::uvec2(2, 0));
        CHECK(tiler.southWestTile(1).coords == glm::uvec2(2, 0));

        CHECK(tiler.northEastTile(2).coords == glm::uvec2(4, 0));
        CHECK(tiler.southWestTile(2).coords == glm::uvec2(4, 0));

        {
            const auto l1_tiles = tiler.generateTiles(1);
            REQUIRE(l1_tiles.size() == 1);
            const auto t = l1_tiles.front();
            CHECK(t.id.zoom_level == 1);
            CHECK(t.id.coords == glm::uvec2(2, 0));
            CHECK(t.gridSize == 64);
            CHECK(t.tileSize == 65);

            constexpr auto tileSizeX = 360 / 4.0;
            constexpr auto tileSizeY = 180 / 2.0;
            CHECK(t.srsBounds.min.x == Approx(0));
            CHECK(t.srsBounds.min.y == Approx(0));
            CHECK(t.srsBounds.max.x == Approx(tileSizeX + tileSizeX / t.gridSize));
            CHECK(t.srsBounds.max.y == Approx(90 + tileSizeY / t.gridSize));
        }
        {
            const auto l2_tiles = tiler.generateTiles(2);
            REQUIRE(l2_tiles.size() == 1);
            const auto t = l2_tiles.front();
            CHECK(t.id.zoom_level == 2);
            CHECK(t.id.coords == glm::uvec2(4, 0));
            CHECK(t.gridSize == 64);
            CHECK(t.tileSize == 65);

            constexpr auto tileSizeX = 360 / 8.0;
            constexpr auto tileSizeY = 180 / 4.0;
            CHECK(t.srsBounds.min.x == Approx(0));
            CHECK(t.srsBounds.min.y == Approx(tileSizeY));
            CHECK(t.srsBounds.max.x == Approx(tileSizeX + tileSizeX / t.gridSize));
            CHECK(t.srsBounds.max.y == Approx(90 + tileSizeY / t.gridSize));
        }
    }

    SECTION("mercator / level 1 and 2 (test with cape horn, on southern and western hemisphere)")
    {
        const auto grid = ctb::GlobalMercator();
        auto dataset = Dataset::open_shared_raster(ALP_TEST_DATA_DIR "/capehorn/small.tif").value();
        const auto tiler = ParallelTiler(grid, dataset->bounds(grid.getSRS()), radix::tile::Border::No);

        CHECK(tiler.northEastTile(1).coords == glm::uvec2(0, 1));
        CHECK(tiler.southWestTile(1).coords == glm::uvec2(0, 1));

        CHECK(tiler.northEastTile(2).coords == glm::uvec2(1, 2));
        CHECK(tiler.southWestTile(2).coords == glm::uvec2(1, 2));

        {
            // https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#1/-5.80/62.29
            const auto l1_tiles = tiler.generateTiles(1);
            REQUIRE(l1_tiles.size() == 1);
            const auto t = l1_tiles.front();
            CHECK(t.id.zoom_level == 1);
            CHECK(t.id.coords == glm::uvec2(0, 1));
            CHECK(t.gridSize == 256);
            CHECK(t.tileSize == 256);

            CHECK(t.srsBounds.min.x == Approx(grid.getExtent().min.x));
            CHECK(t.srsBounds.min.y == Approx(grid.getExtent().min.y));
            CHECK(t.srsBounds.max.x == Approx(0));
            CHECK(t.srsBounds.max.y == Approx(0));
        }
        {
            const auto l2_tiles = tiler.generateTiles(2);
            REQUIRE(l2_tiles.size() == 1);
            const auto t = l2_tiles.front();
            CHECK(t.id.zoom_level == 2);
            CHECK(t.id.coords == glm::uvec2(1, 2));
            CHECK(t.gridSize == 256);
            CHECK(t.tileSize == 256);

            CHECK(t.srsBounds.min.x == Approx(grid.getExtent().min.x + grid.getExtent().width() / 4.0));
            CHECK(t.srsBounds.min.y == Approx(grid.getExtent().min.y + grid.getExtent().height() / 4.0));
            CHECK(t.srsBounds.max.x == Approx(0));
            CHECK(t.srsBounds.max.y == Approx(0));
        }
    }

    SECTION("geodetic / level 1 and 2 (test with cape horn, on southern and western hemisphere)")
    {
        const auto grid = ctb::GlobalGeodetic(64);
        auto dataset = Dataset::open_shared_raster(ALP_TEST_DATA_DIR "/capehorn/small.tif").value();
        const auto tiler = ParallelTiler(grid, dataset->bounds(grid.getSRS()), radix::tile::Border::Yes);

        CHECK(tiler.northEastTile(1).coords == glm::uvec2(1, 1));
        CHECK(tiler.southWestTile(1).coords == glm::uvec2(1, 1));

        CHECK(tiler.northEastTile(2).coords == glm::uvec2(2, 3));
        CHECK(tiler.southWestTile(2).coords == glm::uvec2(2, 3));

        {
            const auto l1_tiles = tiler.generateTiles(1);
            REQUIRE(l1_tiles.size() == 1);
            const auto t = l1_tiles.front();
            CHECK(t.id.zoom_level == 1);
            CHECK(t.id.coords == glm::uvec2(1, 1));
            CHECK(t.gridSize == 64);
            CHECK(t.tileSize == 65);

            constexpr auto tileSizeX = 360 / 4.0;
            constexpr auto tileSizeY = 180 / 2.0;
            CHECK(t.srsBounds.min.x == Approx(-tileSizeX));
            CHECK(t.srsBounds.min.y == Approx(-tileSizeY));
            CHECK(t.srsBounds.max.x == Approx(tileSizeX / 64.0));
            CHECK(t.srsBounds.max.y == Approx(tileSizeX / 64.0));
        }
        {
            const auto l2_tiles = tiler.generateTiles(2);
            REQUIRE(l2_tiles.size() == 1);
            const auto t = l2_tiles.front();
            CHECK(t.id.zoom_level == 2);
            CHECK(t.id.coords == glm::uvec2(2, 3));
            CHECK(t.gridSize == 64);
            CHECK(t.tileSize == 65);

            constexpr auto tileSizeX = 360 / 8.0;
            constexpr auto tileSizeY = 180 / 4.0;
            CHECK(t.srsBounds.min.x == Approx(-2 * tileSizeX));
            CHECK(t.srsBounds.min.y == Approx(-90));
            CHECK(t.srsBounds.max.x == Approx(-tileSizeX + tileSizeX / 64.0));
            CHECK(t.srsBounds.max.y == Approx(-90 + tileSizeY + tileSizeX / 64.0));
        }
    }
}

TEST_CASE("ParallelTiler returns tiles for several zoom levels")
{
    const auto dataset = Dataset(ALP_TEST_DATA_DIR "/austria/at_mgi.tif");
    const auto grid = ctb::GlobalMercator(256);
    const auto tiler = ParallelTiler(grid, dataset.bounds(grid.getSRS()), radix::tile::Border::Yes);

    SECTION("generate from 0 to 7")
    {
        const auto tiles = tiler.generateTiles({ 0, 7 });
        CHECK(!tiles.empty());
        std::map<ctb::i_zoom, unsigned> n_tiles;
        for (const auto& t : tiles) {
            n_tiles[t.id.zoom_level]++;
        }
        REQUIRE(n_tiles.size() == 8);
        CHECK(n_tiles[0] == 1);
        CHECK(n_tiles[1] == 1);
        CHECK(n_tiles[2] == 1);
        CHECK(n_tiles[3] == 1);
        CHECK(n_tiles[4] == 1);
        CHECK(n_tiles[5] == 4);
        CHECK(n_tiles[6] == 6);
        CHECK(n_tiles[7] == 12);

        const auto check = [](const radix::tile::Descriptor& t) {
            return t.tileSize == 257 && t.gridSize == 256;
        };
        CHECK(std::transform_reduce(tiles.begin(), tiles.end(), true, std::logical_and<>(), check) == true);
    }

    SECTION("generate from 4 to 6")
    {
        const auto tiles = tiler.generateTiles({ 4, 6 });
        CHECK(!tiles.empty());
        std::map<ctb::i_zoom, unsigned> n_tiles;
        for (const auto& t : tiles) {
            n_tiles[t.id.zoom_level]++;
        }
        REQUIRE(n_tiles.size() == 3);
        CHECK(n_tiles[4] == 1);
        CHECK(n_tiles[5] == 4);
        CHECK(n_tiles[6] == 6);

        const auto check = [](const radix::tile::Descriptor& t) {
            return t.tileSize == 257 && t.gridSize == 256;
        };
        CHECK(std::transform_reduce(tiles.begin(), tiles.end(), true, std::logical_and<>(), check) == true);
    }
}
