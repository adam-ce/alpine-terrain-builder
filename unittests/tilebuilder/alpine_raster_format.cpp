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

#include <filesystem>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>
#include <numeric>

#include <radix/raster.h>
#include "alpine_raster.h"
#include "ctb/Grid.hpp"

using namespace radix;

radix::tile::Border testTypeValue2Border(bool v)
{
    return v ? radix::tile::Border::Yes : radix::tile::Border::No;
}

TEMPLATE_TEST_CASE("alpine raster format, border ", "", std::true_type, std::false_type)
{
    std::filesystem::remove_all("./unittest_tiles/");

    SECTION("raste write")
    {
        const auto generator = alpine_raster::make_generator(ALP_TEST_DATA_DIR "/austria/at_mgi.tif", "./unittest_tiles/", ctb::Grid::Srs::SphericalMercator, radix::tile::Border::Yes);
        generator.write(radix::tile::Descriptor { {0, glm::uvec2(0, 0)}, {}, int(ctb::Grid::Srs::SphericalMercator), 256, 257 }, radix::Raster<float>({ 257, 257 }));
        CHECK(std::filesystem::exists("./unittest_tiles/0/0/0.png"));

        generator.write(radix::tile::Descriptor { {1, glm::uvec2(2, 3)}, {}, int(ctb::Grid::Srs::SphericalMercator), 256, 257 }, radix::Raster<float>({ 257, 257 }));
        CHECK(std::filesystem::exists("./unittest_tiles/1/2/3.png"));

        // check that a second write doesn't crash
        generator.write(radix::tile::Descriptor { {1, glm::uvec2(2, 3)}, {}, int(ctb::Grid::Srs::SphericalMercator), 256, 257 }, radix::Raster<float>({ 257, 257 }));
        CHECK(std::filesystem::exists("./unittest_tiles/1/2/3.png"));

        // in the best case, we would read back the data and check it. but that's too much work for now.
        // it'll be checked in cesium.js
    }

    SECTION("process all tiles")
    {
        auto generator = alpine_raster::make_generator(ALP_TEST_DATA_DIR "/austria/at_mgi.tif", "./unittest_tiles/", ctb::Grid::Srs::SphericalMercator, testTypeValue2Border(TestType::value));
        generator.setWarnOnMissingOverviews(false);
        generator.process({ 0, 7 });
        const auto tiles = generator.tiler().generateTiles({ 0, 7 });

        const auto check = [](const radix::tile::Descriptor& t) {
            return std::filesystem::exists(fmt::format("./unittest_tiles/{}/{}/{}.png", t.id.zoom_level, t.id.coords.x, t.id.coords.y));
        };
        CHECK(std::transform_reduce(tiles.begin(), tiles.end(), true, std::logical_and<>(), check) == true);
    }
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
    SECTION("process all tiles with max zoom")
    {
        auto generator = alpine_raster::make_generator(ALP_TEST_DATA_DIR "/austria/at_mgi.tif", "./unittest_tiles/", ctb::Grid::Srs::SphericalMercator, testTypeValue2Border(TestType::value));
        generator.setWarnOnMissingOverviews(false);
        generator.process({ 4, 8 });
        const auto tiles = generator.tiler().generateTiles({ 4, 8 });

        const auto check = [](const radix::tile::Descriptor& t) {
            return std::filesystem::exists(fmt::format("./unittest_tiles/{}/{}/{}.png", t.id.zoom_level, t.id.coords.x, t.id.coords.y));
        };
        CHECK(std::transform_reduce(tiles.begin(), tiles.end(), true, std::logical_and<>(), check) == true);
    }
#endif
}
