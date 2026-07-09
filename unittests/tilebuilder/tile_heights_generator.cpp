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

#include "TileHeightsGenerator.h"
#include <catch2/catch_test_macros.hpp>
#include <radix/TileHeights.h>

using namespace radix;

TEST_CASE("TileHeightsGenerator")
{
    const auto base_path = std::filesystem::path("./unittest_tile_heights");
    std::filesystem::remove_all(base_path);

    constexpr auto file_name = "height_data.atb";

    SECTION("mercator") {
        const auto generator = TileHeightsGenerator(ATB_TEST_DATA_DIR "/austria/at_mgi.tif", ctb::Grid::Srs::SphericalMercator, radix::tile::Scheme::Tms, radix::tile::Border::Yes, base_path / file_name);
        generator.run(8);

        const auto heights = TileHeights::read_from(base_path / file_name);
        {
            auto [min, max] = heights.query({ 0, { 0, 0 } });
            CHECK(min == 0);
            CHECK(max > 2500);
        }

        {
            auto [min, max] = heights.query({ 8, { 138, 166 } }); // part of styria, lower and upper austria (https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#8/15.69/47.75)
            CHECK(min > 300);
            CHECK(min < 400);
            CHECK(max > 1500);
            CHECK(max < 2500);
        }
    }

    SECTION("geodetic") {
        const auto generator = TileHeightsGenerator(ATB_TEST_DATA_DIR "/austria/at_mgi.tif", ctb::Grid::Srs::WGS84, radix::tile::Scheme::Tms, radix::tile::Border::Yes, base_path / file_name);
        generator.run(8);

        const auto heights = TileHeights::read_from(base_path / file_name);
        {
            auto [min, max] = heights.query({ 0, { 1, 0 } });
            CHECK(min == 0);
            CHECK(max > 2500);
        }

        {
            auto [min, max] = heights.query({ 8, { 270, 194 } }); // can't check the address easily, because there is no web service showing geodetic tile names.
            CHECK(min > 500);
            CHECK(min < 700);
            CHECK(max > 3600);
            CHECK(max < 3800);
        }
    }
}
