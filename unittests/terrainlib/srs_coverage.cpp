/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace {
using Bounds = radix::geometry::Aabb3d;
using Catch::Approx;

// Regular samples of a box, including its interior.
std::vector<glm::dvec3> box_samples(const Bounds& bounds)
{
    constexpr int horizontal = 40;
    constexpr int vertical = 4;
    std::vector<glm::dvec3> points;
    for (int z = 0; z <= vertical; ++z) {
        for (int y = 0; y <= horizontal; ++y) {
            for (int x = 0; x <= horizontal; ++x) {
                points.push_back(glm::mix(bounds.min, bounds.max, glm::dvec3(x, y, 0) / double(horizontal) + glm::dvec3(0, 0, z / double(vertical))));
            }
        }
    }
    return points;
}

Bounds bounds_of(const std::vector<glm::dvec3>& points)
{
    Bounds bounds;
    for (const auto& point : points) {
        bounds.expand_by(point);
    }
    return bounds;
}

size_t count_uncovered(const std::vector<Bounds>& coverage, const std::vector<glm::dvec3>& points)
{
    return std::ranges::count_if(points,
        [&](const glm::dvec3& point) { return std::ranges::none_of(coverage, [&](const Bounds& bounds) { return bounds.contains_inclusive(point); }); });
}

glm::dvec3 ecef_point(double longitude, double latitude, double height)
{
    return srs::transform_point(srs::wgs84(), srs::ecef(), glm::dvec3(longitude, latitude, height)).value();
}

Bounds cube(const glm::dvec3& centre, double half_size) { return { centre - half_size, centre + half_size }; }

struct Reference {
    std::string name;
    OGRSpatialReference srs;
};
} // namespace

TEST_CASE("ECEF coverage encloses the transformed box", "[srs][coverage]")
{
    struct Case {
        std::string name;
        OGRSpatialReference srs;
        Bounds bounds;
    };
    const std::vector<Case> cases {
        { "geographic_1km", srs::wgs84(), { { 16.36, 48.20, 0 }, { 16.37, 48.21, 1000 } } },
        { "austria", srs::wgs84(), { { 9.5, 46.3, -100 }, { 17.2, 49.1, 4000 } } },
        { "equator", srs::wgs84(), { { -0.37, -0.28, 0 }, { 0.59, 0.44, 1000 } } },
        { "near_pole", srs::wgs84(), { { 10, 89.0, 0 }, { 20, 89.9, 500 } } },
        { "antimeridian", srs::wgs84(), { { 170, -5, 0 }, { 190, 5, 100 } } },
        { "mercator_10km", srs::webmercator(), { { 1810000, 6130000, 0 }, { 1820000, 6140000, 1000 } } },
        { "austria_lambert", srs::from_epsg(31287).value(), { { 400000, 400000, 0 }, { 500000, 480000, 3000 } } },
        { "utm_33n", srs::from_epsg(32633).value(), { { 500000, 5300000, 0 }, { 600000, 5400000, 3000 } } },
    };
    for (const auto& scenario : cases) {
        CAPTURE(scenario.name);
        const auto coverage = srs::ecef_coverage(scenario.srs, scenario.bounds);
        REQUIRE(coverage);
        const auto samples = srs::transform_points(scenario.srs, srs::ecef(), box_samples(scenario.bounds)).value();
        CHECK(count_uncovered({ *coverage }, samples) == 0);
        // Going through a longitude/latitude box loosens projected boxes by their rotation
        // against the meridians, a few percent here.
        const auto sampled = bounds_of(samples);
        CAPTURE(coverage->size() / sampled.size());
        CHECK(glm::all(glm::lessThanEqual(coverage->size(), sampled.size() * 1.05 + 0.01)));
    }
}

TEST_CASE("ECEF coverage of geographic boxes is exact", "[srs][coverage]")
{
    SECTION("extrema at corners")
    {
        const Bounds austria { { 9.5, 46.3, -100 }, { 17.2, 49.1, 4000 } };
        Bounds corners;
        for (const double longitude : { austria.min.x, austria.max.x }) {
            for (const double latitude : { austria.min.y, austria.max.y }) {
                for (const double height : { austria.min.z, austria.max.z }) {
                    corners.expand_by(ecef_point(longitude, latitude, height));
                }
            }
        }
        const auto coverage = srs::ecef_coverage(srs::wgs84(), austria);
        REQUIRE(coverage);
        for (int axis = 0; axis < 3; ++axis) {
            CHECK(coverage->min[axis] == Approx(corners.min[axis]).margin(0.01));
            CHECK(coverage->max[axis] == Approx(corners.max[axis]).margin(0.01));
        }
    }
    SECTION("extremum inside the top face, at the equator and the prime meridian")
    {
        const auto coverage = srs::ecef_coverage(srs::wgs84(), { { -0.37, -0.28, 0 }, { 0.59, 0.44, 1000 } });
        REQUIRE(coverage);
        CHECK(coverage->max.x == Approx(6378137.0 + 1000).margin(0.01));
    }
    SECTION("extremum inside an edge, at the prime meridian")
    {
        const auto coverage = srs::ecef_coverage(srs::wgs84(), { { -1, 10, 0 }, { 1, 11, 100 } });
        REQUIRE(coverage);
        CHECK(coverage->max.x == Approx(ecef_point(0, 10, 100).x).margin(0.01));
    }
}

TEST_CASE("Coverage of ECEF boxes encloses the transformed box", "[srs][coverage]")
{
    const auto vienna = ecef_point(16.37, 48.21, 250);
    const std::vector<Reference> references {
        { "wgs84", srs::wgs84() },
        { "webmercator", srs::webmercator() },
        { "mgi", srs::mgi() },
        { "austria_lambert", srs::from_epsg(31287).value() },
        { "utm_33n", srs::from_epsg(32633).value() },
    };
    for (const auto& [box_name, box] :
        { std::pair { "1km", cube(vienna, 500) }, std::pair { "100km", cube(vienna, 50000) }, std::pair { "1000km", cube(vienna, 500000) } }) {
        for (const auto& reference : references) {
            CAPTURE(box_name, reference.name);
            const auto coverage = srs::ecef2srs_coverage(box, reference.srs);
            REQUIRE(coverage);
            REQUIRE(coverage->size() == 1);
            const auto samples = srs::transform_points(srs::ecef(), reference.srs, box_samples(box)).value();
            CHECK(count_uncovered(*coverage, samples) == 0);
            // The longitude/latitude box is rotated against the projected axes by the meridian
            // convergence, about 2 degrees at Vienna for UTM 33N and Austria Lambert.
            const auto sampled = bounds_of(samples);
            const auto looseness = (coverage->front().size() - sampled.size()) / sampled.size();
            CAPTURE(looseness);
            CHECK(glm::all(glm::lessThanEqual(looseness, glm::dvec3(0.1))));
        }
    }
}

TEST_CASE("Coverage of ECEF boxes is exact in WGS84", "[srs][coverage]")
{
    SECTION("extrema at corners")
    {
        const auto box = cube(ecef_point(16.37, 48.21, 250), 50000);
        const auto coverage = srs::ecef2srs_coverage(box, srs::wgs84());
        REQUIRE(coverage);
        REQUIRE(coverage->size() == 1);
        // The extrema are at corners of the box, which are among the samples.
        const auto sampled = bounds_of(srs::transform_points(srs::ecef(), srs::wgs84(), box_samples(box)).value());
        for (int axis = 0; axis < 3; ++axis) {
            const double margin = axis < 2 ? 1e-8 : 0.01;
            CHECK(coverage->front().min[axis] == Approx(sampled.min[axis]).margin(margin));
            CHECK(coverage->front().max[axis] == Approx(sampled.max[axis]).margin(margin));
        }
    }
    SECTION("height minimum inside a face, at the equator")
    {
        const auto coverage = srs::ecef2srs_coverage(cube({ 6378137, 0, 0 }, 50000), srs::wgs84());
        REQUIRE(coverage);
        REQUIRE(coverage->size() == 1);
        CHECK(coverage->front().min.z == Approx(-50000).margin(0.01));
    }
}

TEST_CASE("Coverage of ECEF boxes stays within the bounds of the SRS", "[srs][coverage]")
{
    SECTION("antimeridian")
    {
        const auto box = cube({ -6378137, 0, 0 }, 50000);
        for (const auto& reference : { srs::wgs84(), srs::webmercator() }) {
            const auto coverage = srs::ecef2srs_coverage(box, reference);
            REQUIRE(coverage);
            REQUIRE(coverage->size() == 2);
            const double half_width = reference.IsGeographic() ? 180 : srs::webmercator_half_extent;
            for (const auto& bounds : *coverage) {
                CHECK(bounds.min.x >= -half_width);
                CHECK(bounds.max.x <= half_width);
            }
            const auto samples = srs::transform_points(srs::ecef(), reference, box_samples(box)).value();
            CHECK(count_uncovered(*coverage, samples) == 0);
        }
    }
    SECTION("north pole")
    {
        const auto box = cube({ 0, 0, 6356752.3 }, 50000);
        const auto geographic = srs::ecef2srs_coverage(box, srs::wgs84());
        REQUIRE(geographic);
        REQUIRE(geographic->size() == 1);
        CHECK(geographic->front().min.x == -180);
        CHECK(geographic->front().max.x == 180);
        CHECK(geographic->front().max.y == 90);
        CHECK(count_uncovered(*geographic, srs::transform_points(srs::ecef(), srs::wgs84(), box_samples(box)).value()) == 0);
        const auto mercator = srs::ecef2srs_coverage(box, srs::webmercator());
        REQUIRE(mercator);
        CHECK(mercator->empty());
    }
    SECTION("centre of the earth")
    {
        const auto coverage = srs::ecef2srs_coverage(cube(glm::dvec3(0), 1000), srs::wgs84());
        REQUIRE_FALSE(coverage);
        CHECK(coverage.error().code() == Error::Code::Unsupported);
    }
}

TEST_CASE("Coverage functions handle ECEF, compound SRSes and invalid bounds", "[srs][coverage]")
{
    const Bounds box = cube(ecef_point(16.37, 48.21, 250), 500);
    CHECK(srs::ecef_coverage(srs::ecef(), box).value() == box);
    const auto unchanged = srs::ecef2srs_coverage(box, srs::ecef()).value();
    REQUIRE(unchanged.size() == 1);
    CHECK(unchanged.front() == box);

    const auto compound = srs::from_user_input("EPSG:4326+3855").value();
    REQUIRE(compound.IsCompound());
    const auto forward = srs::ecef_coverage(compound, { { 16.36, 48.20, 0 }, { 16.37, 48.21, 1000 } });
    REQUIRE_FALSE(forward);
    CHECK(forward.error().code() == Error::Code::Unsupported);
    const auto backward = srs::ecef2srs_coverage(box, compound);
    REQUIRE_FALSE(backward);
    CHECK(backward.error().code() == Error::Code::Unsupported);

    for (const double invalid : { -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
        Bounds bad = box;
        bad.max.x = invalid;
        const auto forward_result = srs::ecef_coverage(srs::wgs84(), bad);
        REQUIRE_FALSE(forward_result);
        CHECK(forward_result.error().code() == Error::Code::InvalidInput);
        const auto backward_result = srs::ecef2srs_coverage(bad, srs::wgs84());
        REQUIRE_FALSE(backward_result);
        CHECK(backward_result.error().code() == Error::Code::InvalidInput);
    }
}
