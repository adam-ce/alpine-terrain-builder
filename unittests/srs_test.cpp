/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <last name at cg tuwien ac at>
 * Copyright (C) 2022 alpinemaps.org
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

#include "srs.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <ogr_spatialref.h>
#include <vector>

using namespace radix;
using Catch::Approx;

TEST_CASE("ECEF conversinos")
{
    OGRSpatialReference webmercator;
    webmercator.importFromEPSG(3857);
    webmercator.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference wgs84;
    wgs84.importFromEPSG(4326);
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    SECTION("single point")
    {
        // comparison against https://www.oc.nps.edu/oc2902w/coord/llhxyz.htm
        const auto steffl_wgs84 = glm::dvec3(16.372489, 48.208814, 171.28);
        const auto steffl_webmercator = srs::transform_point(wgs84, webmercator, steffl_wgs84);
        {
            const auto steffl_ecef = srs::toECEF(wgs84, steffl_wgs84);
            CHECK(steffl_ecef.x == Approx(4085862.0));
            CHECK(steffl_ecef.y == Approx(1200403.0));
            CHECK(steffl_ecef.z == Approx(4732509.0));
        }
        {
            const auto steffl_ecef = srs::toECEF(webmercator, steffl_webmercator);
            CHECK(steffl_ecef.x == Approx(4085862.0));
            CHECK(steffl_ecef.y == Approx(1200403.0));
            CHECK(steffl_ecef.z == Approx(4732509.0));
        }
    }

    SECTION("multiple points")
    {
        // comparison against https://www.oc.nps.edu/oc2902w/coord/llhxyz.htm
        const auto steffl_wgs84 = glm::dvec3(16.372489, 48.208814, 171.28);
        const auto grossglockner_wgs84 = glm::dvec3(12.694504, 47.073913, 3798.0);

        const auto [steffl_ecef, grossglockner_ecef] = srs::toECEF(wgs84, steffl_wgs84, grossglockner_wgs84);
        {
            CHECK(steffl_ecef.x == Approx(4085862.0));
            CHECK(steffl_ecef.y == Approx(1200403.0));
            CHECK(steffl_ecef.z == Approx(4732509.0));
        }
        {
            CHECK(grossglockner_ecef.x == Approx(4247824.0));
            CHECK(grossglockner_ecef.y == Approx(956860.0));
            CHECK(grossglockner_ecef.z == Approx(4650146.0));
        }
    }

    SECTION("point array")
    {
        // comparison against https://www.oc.nps.edu/oc2902w/coord/llhxyz.htm
        const std::vector points = { glm::dvec3(16.372489, 48.208814, 171.28),
            glm::dvec3(12.694504, 47.073913, 3798.0) };
        const auto ecef_points = srs::toECEF(wgs84, points);
        CHECK(ecef_points[0].x == Approx(4085862.0));
        CHECK(ecef_points[0].y == Approx(1200403.0));
        CHECK(ecef_points[0].z == Approx(4732509.0));

        CHECK(ecef_points[1].x == Approx(4247824.0));
        CHECK(ecef_points[1].y == Approx(956860.0));
        CHECK(ecef_points[1].z == Approx(4650146.0));
    }
}
