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

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Image.h"

using Catch::Approx;

TEST_CASE("image")
{
    SECTION("iteration")
    {
        HeightData d { 40, 60 };
        REQUIRE(d.size() == 40 * 60);
        REQUIRE(d.height() == 60);
        REQUIRE(d.width() == 40);
        int v = 0;
        std::ranges::for_each(d, [&](auto& d) { d = float(v++); });

        v = 0;
        for (unsigned r = 0; r < d.height(); ++r) {
            for (unsigned c = 0; c < d.width(); ++c) {
                REQUIRE(d.pixel(r, c) == Approx(float(v++)));
            }
        }
    }

    SECTION("conversion")
    {
        HeightData d { 40, 60 };
        int v_init = 0;
        std::ranges::for_each(d, [&](auto& d) { d = float(v_init++); });

        Image<glm::u8vec3> image = image::transformImage(d, [&](auto v) { const auto b =  uchar(255.F * v / float(v_init)); return glm::u8vec3(b, b, b); });
        const auto max = float(v_init);
        v_init = 0;
        for (unsigned r = 0; r < d.height(); ++r) {
            for (unsigned c = 0; c < d.width(); ++c) {
                const auto t = uchar(float(v_init) * 255.F / max);
                REQUIRE(image.pixel(r, c).x == t);
                REQUIRE(image.pixel(r, c).y == t);
                REQUIRE(image.pixel(r, c).z == t);
                v_init++;
            }
        }
//        image::saveImageAsPng(image, "/home/madam/Documents/work/tuw/alpinemaps/tmp/test.png");
    }
}
