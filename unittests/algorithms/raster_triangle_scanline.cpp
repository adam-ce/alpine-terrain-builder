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

#include "algorithms/raster_triangle_scanline.h"
#include "../catch2_helpers.h"
#include "tntn/Raster.h"
#include <glm/glm.hpp>

TEST_CASE("raster triangle goes through pixels exactly once (small rect)")
{
    constexpr auto width = 3;
    constexpr auto height = 3;
    tntn::Raster<int> raster(width, height);
    const auto scan_tri = [&](const glm::uvec2& coords, int value) {
        REQUIRE(coords.x < width);
        REQUIRE(coords.y < height);
        CHECK(value == 0);
        raster.value(coords.y, coords.x)++;
    };
    raster.set_all(0);
    /* d----c
     * | e  |
     * a----b
     */
    glm::uvec2 a = { 0, 0 };
    glm::uvec2 b = { width - 1, 0 };
    glm::uvec2 c = { width - 1, height - 1 };
    glm::uvec2 d = { 0, height - 1 };
    glm::uvec2 e = { 1, 1 };

    //  SECTION("upper left triangle") {
    //    const auto check_tri = [&](){
    //      CHECK(raster.value(0, 0) == 0);
    //      CHECK(raster.value(0, 1) == 0);
    //      CHECK(raster.value(0, 2) == 0);
    //      CHECK(raster.value(1, 0) == 1);
    //      CHECK(raster.value(1, 1) == 0);
    //      CHECK(raster.value(1, 2) == 0);
    //      CHECK(raster.value(2, 0) == 1);
    //      CHECK(raster.value(2, 1) == 1);
    //      CHECK(raster.value(2, 2) == 0);
    //    };
    //    raster::triangle_scanline(raster, a, c, d, scan_tri);
    //    check_tri();
    //    raster.set_all(0);
    //    raster::triangle_scanline(raster, c, d, a, scan_tri);
    //    check_tri();
    //    raster.set_all(0);
    //    raster::triangle_scanline(raster, d, a, c, scan_tri);
    //    check_tri();
    //    raster.set_all(0);
    //  }

    SECTION("lower right triangle")
    {
        const auto check_tri = [&]() {
            CHECK(raster.value(0, 0) == 1);
            CHECK(raster.value(0, 1) == 1);
            CHECK(raster.value(0, 2) == 1);
            CHECK(raster.value(1, 0) == 0);
            CHECK(raster.value(1, 1) == 1);
            CHECK(raster.value(1, 2) == 1);
            CHECK(raster.value(2, 0) == 0);
            CHECK(raster.value(2, 1) == 0);
            CHECK(raster.value(2, 2) == 1);
        };

        raster::triangle_scanline(raster, a, b, c, scan_tri);
        check_tri();
        raster.set_all(0);
        raster::triangle_scanline(raster, b, c, a, scan_tri);
        check_tri();
        raster.set_all(0);
        raster::triangle_scanline(raster, c, a, b, scan_tri);
        check_tri();
        raster.set_all(0);
    }

    SECTION("fill")
    {
        raster::triangle_scanline(raster, a, b, e, scan_tri);
        raster::triangle_scanline(raster, e, b, c, scan_tri);
        raster::triangle_scanline(raster, e, c, d, scan_tri);
        raster::triangle_scanline(raster, d, a, e, scan_tri);
        for (const auto& v : raster.asVector()) {
            CHECK(v == 1);
        }
        raster.set_all(0);
    }
}

TEST_CASE("raster triangle goes through pixels exactly once (larger rect)")
{
    constexpr auto width = 17;
    constexpr auto height = 20;
    tntn::Raster<int> raster(width, height);
    const auto scan_tri = [&](const glm::uvec2& coords, int value) {
        REQUIRE(coords.x < width);
        REQUIRE(coords.y < height);
        if (value != 0)
            CHECK(value == 0);
        raster.value(coords.y, coords.x)++;
    };
    raster.set_all(0);
    /*
     * g----h----i
     * |  / | \  |
     * d----e----f
     * | /  | \  |
     * a----b----c
     */
    glm::uvec2 a = { 0, 0 };
    glm::uvec2 b = { 4, 0 };
    glm::uvec2 c = { width - 1, 0 };
    glm::uvec2 d = { 0, 12 };
    glm::uvec2 e = { 10, 10 };
    glm::uvec2 f = { width - 1, 11 };
    glm::uvec2 g = { 0, height - 1 };
    glm::uvec2 h = { 1, height - 1 };
    glm::uvec2 i = { width - 1, height - 1 };
    SECTION("fill")
    {
        raster::triangle_scanline(raster, a, b, e, scan_tri);
        raster::triangle_scanline(raster, a, e, d, scan_tri);

        raster::triangle_scanline(raster, b, c, e, scan_tri);
        raster::triangle_scanline(raster, f, e, c, scan_tri);

        raster::triangle_scanline(raster, g, d, h, scan_tri);
        raster::triangle_scanline(raster, h, d, e, scan_tri);

        raster::triangle_scanline(raster, i, h, f, scan_tri);
        raster::triangle_scanline(raster, f, h, e, scan_tri);
        for (const auto& v : raster.asVector()) {
            CHECK(v == 1);
        }
        raster.set_all(0);
    }
}
