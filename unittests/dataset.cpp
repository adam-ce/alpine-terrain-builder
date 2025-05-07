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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Dataset.h"
#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"
#include "ctb/types.hpp"
#include "srs.h"

using namespace radix;
using Catch::Approx;

void checkBounds(const tile::SrsBounds& a, const tile::SrsBounds& b)
{
    REQUIRE(a.height() > 0);
    REQUIRE(a.width() > 0);
    REQUIRE(b.height() > 0);
    REQUIRE(b.width() > 0);

    const auto heightErrorIn = std::abs(a.height() - b.height()) / a.height();
    const auto widthErrorIn = std::abs(a.width() - b.width()) / a.width();
    //  fmt::print("height error = {}, width error = {}\n", heightErrorIn, widthErrorIn);
    REQUIRE(heightErrorIn < 0.001);
    REQUIRE(widthErrorIn < 0.001);
}

TEST_CASE("datasets are as expected")
{
    auto d_mgi = Dataset(ATB_TEST_DATA_DIR "/austria/at_mgi.tif");
    auto d_wgs84 = Dataset(ATB_TEST_DATA_DIR "/austria/at_wgs84.tif");

    OGRSpatialReference webmercator;
    webmercator.importFromEPSG(3857);
    webmercator.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference wgs84;
    wgs84.importFromEPSG(4326);
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    SECTION("file name")
    {
        CHECK(d_mgi.name() == "at_mgi");
        CHECK(d_wgs84.name() == "at_wgs84");
    }

    SECTION("SRS")
    {
        REQUIRE_FALSE(d_mgi.srs().IsEmpty());
        REQUIRE_FALSE(d_wgs84.srs().IsEmpty());
        const auto wgs84 = d_wgs84.srs();
        REQUIRE_FALSE(d_mgi.srs().IsSame(&wgs84));
    }

    SECTION("bounds")
    {
        {
            //      fmt::print("webmercator: \n");
            checkBounds(d_mgi.bounds(webmercator), d_wgs84.bounds(webmercator));
        }
        {
            //      fmt::print("d_wgs84: \n");
            checkBounds(d_mgi.bounds(d_wgs84.srs()), d_wgs84.bounds(d_wgs84.srs()));
        }
        //    {
        //      // doesn't work: wgs84 was created by projecting the original mgi data. since wgs84 is warped, the projection creates a border.
        //      //               when backprojecting to mgi, there is still a border in the raster data (but widthout height information).
        //      //               this is not an error in the code, and impossible to avoid when rerasterising warped raster data
        //      fmt::print("d_mgi: \n");
        //      checkBounds(d_mgi.bounds(d_mgi.srs()), d_wgs84.bounds(d_mgi.srs()));
        //    }
    }
    SECTION("resolution")
    {
        CHECK(d_mgi.widthInPixels() == 620);
        CHECK(d_mgi.heightInPixels() == 350);
        CHECK(d_mgi.n_bands() == 1);

        const auto northern = 49.222158096;
        const auto southern = 46.077736128;
        const auto eastern = 17.58967912;
        const auto western = 9.323270147;

        CHECK(d_mgi.pixelWidthIn(wgs84) == Approx((eastern - western) / 620));
        CHECK(d_mgi.pixelHeightIn(wgs84) == Approx((northern - southern) / 350));
        CHECK(d_mgi.gridResolution(wgs84) == Approx((northern - southern) / 350));

        const auto wgs84_grid = ctb::GlobalGeodetic(256);
        CHECK(wgs84_grid.zoomForResolution(d_mgi.gridResolution(wgs84)) == 7);

        const auto webmercator_grid = ctb::GlobalMercator();
        CHECK(webmercator_grid.zoomForResolution(d_mgi.gridResolution(webmercator)) == 7);
    }
}

TEST_CASE("bbox width pixels")
{
    auto d_mgi = Dataset(ATB_TEST_DATA_DIR "/austria/at_mgi.tif");
    auto d_wgs84 = Dataset(ATB_TEST_DATA_DIR "/austria/at_wgs84.tif");

    OGRSpatialReference webmercator;
    webmercator.importFromEPSG(3857);
    webmercator.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    REQUIRE(d_mgi.widthInPixels(d_mgi.bounds(), d_mgi.srs()) == Approx(620.0));
    REQUIRE(d_mgi.heightInPixels(d_mgi.bounds(), d_mgi.srs()) == Approx(350.0));
    REQUIRE(d_mgi.widthInPixels(srs::non_exact_bounds_transform(d_mgi.bounds(), d_mgi.srs(), webmercator), webmercator) == Approx(620.0));
    REQUIRE(d_mgi.heightInPixels(srs::non_exact_bounds_transform(d_mgi.bounds(), d_mgi.srs(), webmercator), webmercator) == Approx(350.0));

    auto adjust_bounds = [](auto bounds) {
        const auto unadjusted_width = bounds.width();
        const auto unadjusted_height = bounds.height();
        bounds.min += glm::dvec2{ unadjusted_width * 0.2, unadjusted_height * 0.3 };
        bounds.max -= glm::dvec2{ unadjusted_width * 0.1, unadjusted_height * 0.2 };
        return bounds;
    };

    REQUIRE(d_wgs84.widthInPixels(adjust_bounds(d_wgs84.bounds()), d_wgs84.srs()) == Approx(620.0 * 0.7));
    REQUIRE(d_wgs84.heightInPixels(adjust_bounds(d_wgs84.bounds()), d_wgs84.srs()) == Approx(350.0 * 0.5));

    const auto webmercator_bounds = srs::non_exact_bounds_transform(d_wgs84.bounds(), d_wgs84.srs(), webmercator);
    REQUIRE(d_wgs84.widthInPixels(webmercator_bounds, webmercator) == Approx(620.0));
    REQUIRE(d_wgs84.heightInPixels(webmercator_bounds, webmercator) == Approx(350.0));

    const auto webmercator_adjusted_bounds = adjust_bounds(webmercator_bounds);
    REQUIRE(d_wgs84.widthInPixels(webmercator_adjusted_bounds, webmercator) == Approx(620.0 * 0.7));
    REQUIRE(d_wgs84.heightInPixels(webmercator_adjusted_bounds, webmercator) == Approx(350.0 * 0.5));
}
