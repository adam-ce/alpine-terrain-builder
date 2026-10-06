// Copyright (C) 2026 Adam Celarek-Litofcenko
// SPDX-License-Identifier: GPL-3.0-or-later

#include "srs.h"
#include "srs/detail.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cpl_error.h>
#include <ogr_spatialref.h>

namespace {
struct BoundsCase {
    std::string name;
    std::string source;
    std::string target;
    std::array<double, 4> bounds;
    OSRAxisMappingStrategy axis_mapping = OAMS_TRADITIONAL_GIS_ORDER;
};

void compare_bounds(const BoundsCase& scenario, int samples)
{
    CAPTURE(scenario.name, samples);
    auto source = srs::from_user_input(scenario.source).value();
    auto target = srs::from_user_input(scenario.target).value();
    source.SetAxisMappingStrategy(scenario.axis_mapping);
    target.SetAxisMappingStrategy(scenario.axis_mapping);
    std::unique_ptr<OGRCoordinateTransformation> reference(OGRCreateCoordinateTransformation(&source, &target));
    std::unique_ptr<OGRCoordinateTransformation> extracted(OGRCreateCoordinateTransformation(&source, &target));
    REQUIRE(reference);
    REQUIRE(extracted);
    CPLErrorHandlerPusher quiet(CPLQuietErrorHandler);
    const auto& b = scenario.bounds;
    std::array<double, 4> expected {}, actual {};
    const int expected_success = reference->TransformBounds(b[0], b[1], b[2], b[3], &expected[0], &expected[1], &expected[2], &expected[3], samples);
    const auto expected_error = CPLGetLastErrorType();
    const int actual_success
        = srs::detail::ogr_transform_bounds(extracted.get(), b[0], b[1], b[2], b[3], &actual[0], &actual[1], &actual[2], &actual[3], samples);
    CHECK(actual_success == expected_success);
    CHECK(CPLGetLastErrorType() == expected_error);
    for (size_t i = 0; i < actual.size(); ++i) {
        CAPTURE(i, expected[i], actual[i]);
        if (std::isnan(expected[i])) {
            CHECK(std::isnan(actual[i]));
        } else if (std::isinf(expected[i])) {
            CHECK(actual[i] == expected[i]);
        } else {
            CHECK(actual[i] == Catch::Approx(expected[i]).epsilon(1e-10).margin(1e-7));
        }
    }
}
} // namespace

TEST_CASE("Cached projected bounds agree with GDAL", "[srs][bounds]")
{
    const std::array cases {
        BoundsCase { "Austria Lambert", "EPSG:4326", "EPSG:31287", { 12.6, 47.0, 12.61, 47.01 } },
        BoundsCase { "Austria extent", "EPSG:4326", "EPSG:31287", { 9.5, 46.3, 17.2, 49.1 } },
        BoundsCase { "UTM", "EPSG:4326", "EPSG:32633", { 14, 46, 17, 49 } },
        BoundsCase { "Mercator", "EPSG:4326", "EPSG:3857", { -170, -80, 170, 80 } },
        BoundsCase { "Mercator polar limits", "EPSG:4326", "EPSG:3857", { -180, -90, 180, 90 } },
        BoundsCase { "north pole", "EPSG:4326", "EPSG:3413", { -180, 80, 180, 90 } },
        BoundsCase { "south pole", "EPSG:4326", "EPSG:3031", { -180, -90, 180, -80 } },
        BoundsCase { "pole and seam", "EPSG:4326", "ESRI:53037", { -180, 70, 180, 90 } },
        BoundsCase { "shifted seam", "EPSG:4326", "+proj=sinu +lon_0=10 +datum=WGS84 +units=m +type=crs", { -175, -80, -165, 80 } },
        BoundsCase { "negative meridian", "EPSG:4326", "+proj=sinu +lon_0=-10 +datum=WGS84 +units=m +type=crs", { 165, -80, 175, 80 } },
        BoundsCase { "crossing antimeridian", "EPSG:4326", "EPSG:3857", { 170, -10, -170, 10 } },
        BoundsCase { "unwrapped antimeridian", "EPSG:4326", "EPSG:3857", { 170, -10, 190, 10 } },
        BoundsCase { "point", "EPSG:4326", "EPSG:31287", { 12.6, 47, 12.6, 47 } },
        BoundsCase { "line", "EPSG:4326", "EPSG:31287", { 12.6, 47, 12.6, 48 } },
        BoundsCase { "projected source", "EPSG:3857", "EPSG:31287", { 1400000, 5900000, 1500000, 6000000 } },
        BoundsCase { "different geographic datum", "EPSG:4312", "EPSG:32633", { 14, 46, 17, 49 } },
        BoundsCase { "identity", "EPSG:31287", "EPSG:31287", { 300000, 300000, 400000, 400000 } },
        BoundsCase { "geographic output", "EPSG:3857", "EPSG:4326", { 1400000, 5900000, 1500000, 6000000 } },
        BoundsCase { "geographic pole", "EPSG:3413", "EPSG:4326", { -100000, -100000, 100000, 100000 } },
        BoundsCase { "authority axes", "EPSG:4326", "EPSG:31287", { 47, 12.6, 47.01, 12.61 }, OAMS_AUTHORITY_COMPLIANT },
        BoundsCase { "grads", "EPSG:4807", "EPSG:3857", { -5, 50, 10, 60 } },
        BoundsCase { "partial domain", "EPSG:4326", "+proj=ortho +lat_0=0 +lon_0=0 +datum=WGS84 +type=crs", { -10, -20, 100, 20 } },
        BoundsCase { "outside domain", "EPSG:4326", "+proj=ortho +lat_0=0 +lon_0=0 +datum=WGS84 +type=crs", { 100, -20, 150, 20 } },
    };
    for (const auto& scenario : cases) {
        for (int samples : { 0, 2, 21, 257 }) {
            compare_bounds(scenario, samples);
        }
    }
}

TEST_CASE("Cached bounds preserve GDAL failure handling", "[srs][bounds]")
{
    BoundsCase scenario { "invalid densification", "EPSG:4326", "EPSG:31287", { 12.6, 47, 12.61, 47.01 } };
    for (int samples : { -1, 10001 }) {
        compare_bounds(scenario, samples);
    }
    scenario.name = "inverted latitude";
    scenario.bounds = { 12, 48, 13, 47 };
    compare_bounds(scenario, 21);
    scenario.name = "nonfinite bounds";
    scenario.bounds = { 12, 47, (std::numeric_limits<double>::infinity)(), 48 };
    compare_bounds(scenario, 21);
}

TEST_CASE("Cached bounds include interior points at a shifted projection seam", "[srs][bounds]")
{
    const auto source = srs::wgs84();
    const auto target = srs::from_user_input("+proj=sinu +lon_0=10 +datum=WGS84 +units=m +type=crs").value();
    const auto transform = srs::transformation(source, target).value();
    std::array<double, 4> bounds {};
    REQUIRE(srs::detail::ogr_transform_bounds(transform.get(), -175, -80, -165, 80, &bounds[0], &bounds[1], &bounds[2], &bounds[3], 257));
    for (double longitude : { -170 - 1e-8, -170 + 1e-8 }) {
        const auto point = srs::transform_point(transform.get(), glm::dvec2(longitude, 0)).value();
        CHECK(point.x >= bounds[0] - 1e-7);
        CHECK(point.x <= bounds[2] + 1e-7);
        CHECK(point.y >= bounds[1]);
        CHECK(point.y <= bounds[3]);
    }
}

TEST_CASE("Projected bounds transformation benchmark", "[srs][!benchmark][.]")
{
    const auto transform = srs::transformation(srs::wgs84(), srs::from_epsg(31287).value()).value();
    std::array<double, 4> bounds {};
    REQUIRE(srs::detail::ogr_transform_bounds(transform.get(), 12.6, 47, 12.61, 47.01, &bounds[0], &bounds[1], &bounds[2], &bounds[3], 257));
    BENCHMARK("GDAL TransformBounds")
    {
        transform->TransformBounds(12.6, 47, 12.61, 47.01, &bounds[0], &bounds[1], &bounds[2], &bounds[3], 257);
        return bounds;
    };
    BENCHMARK("cached projected bounds")
    {
        srs::detail::ogr_transform_bounds(transform.get(), 12.6, 47, 12.61, 47.01, &bounds[0], &bounds[1], &bounds[2], &bounds[3], 257);
        return bounds;
    };
}
