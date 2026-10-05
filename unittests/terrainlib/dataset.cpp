/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek-Litofcenko
 * Copyright (C) 2022 Martin Braunsperger
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

#include "../catch2_helpers.h"

#include <catch2/catch_approx.hpp>
#include <gdal_priv.h>

#include "Dataset.h"
#include "init.h"
#include "srs.h"

using namespace radix;
using Catch::Approx;

void checkBounds(const radix::tile::SrsBounds &a, const radix::tile::SrsBounds &b) {
    CHECK(a.height() > 0);
    CHECK(a.width() > 0);
    CHECK(b.height() > 0);
    CHECK(b.width() > 0);

    const auto heightErrorIn = std::abs(a.height() - b.height()) / a.height();
    const auto widthErrorIn = std::abs(a.width() - b.width()) / a.width();
    //  fmt::print("height error = {}, width error = {}\n", heightErrorIn, widthErrorIn);
    // Reprojected coverage includes a guard band of 1/258 of the extent per side.
    CHECK(heightErrorIn < 0.01);
    CHECK(widthErrorIn < 0.01);
}

TEST_CASE("datasets are as expected") {
    auto d_mgi = Dataset(ALP_TEST_DATA_DIR "/austria/at_mgi.tif");
    auto d_wgs84 = Dataset(ALP_TEST_DATA_DIR "/austria/at_wgs84.tif");

    SECTION("file name") {
        CHECK(d_mgi.name() == "at_mgi");
        CHECK(d_wgs84.name() == "at_wgs84");
    }

    SECTION("SRS") {
        REQUIRE_FALSE(d_mgi.srs().value().IsEmpty());
        REQUIRE_FALSE(d_wgs84.srs().value().IsEmpty());
        const auto wgs84 = d_wgs84.srs().value();
        REQUIRE_FALSE(d_mgi.srs().value().IsSame(&wgs84));
    }

    SECTION("coverage")
    {
        {
            const auto mgi_coverage = d_mgi.mercator_coverage().value();
            const auto wgs84_coverage = d_wgs84.mercator_coverage().value();
            REQUIRE(mgi_coverage.size() == 1);
            REQUIRE(wgs84_coverage.size() == 1);
            checkBounds(mgi_coverage.front(), wgs84_coverage.front());
        }
        {
            const auto mgi_coverage = d_mgi.geographic_coverage().value();
            const auto wgs84_coverage = d_wgs84.geographic_coverage().value();
            REQUIRE(mgi_coverage.size() == 1);
            REQUIRE(wgs84_coverage.size() == 1);
            checkBounds(mgi_coverage.front(), wgs84_coverage.front());
        }
    }
    SECTION("resolution") {
        CHECK(d_mgi.widthInPixels() == 620);
        CHECK(d_mgi.heightInPixels() == 350);
        CHECK(d_mgi.n_bands() == 1);
    }
}

TEST_CASE("vector datasets use the layer spatial reference") {
    initialize_gdal_once();
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("Memory");
    REQUIRE(driver != nullptr);

    GDALDataset *raw_dataset = driver->Create("", 0, 0, 0, GDT_Unknown, nullptr);
    REQUIRE(raw_dataset != nullptr);

    OGRSpatialReference expected_srs;
    REQUIRE(expected_srs.importFromEPSG(31287) == OGRERR_NONE);
    expected_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    REQUIRE(raw_dataset->CreateLayer("mask", &expected_srs, wkbPolygon, nullptr) != nullptr);

    Dataset dataset(raw_dataset);
    const OGRSpatialReference actual_srs = dataset.srs().value();
    CHECK(actual_srs.IsSame(&expected_srs));
}

namespace {
Dataset memory_raster(std::array<double, 6> geotransform, glm::uvec2 size, int epsg)
{
    initialize_gdal_once();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("MEM");
    REQUIRE(driver != nullptr);
    Dataset dataset(driver->Create("", int(size.x), int(size.y), 1, GDT_Float32, nullptr));
    REQUIRE(dataset.gdalDataset()->SetGeoTransform(geotransform.data()) == CE_None);
    OGRSpatialReference reference;
    REQUIRE(reference.importFromEPSG(epsg) == OGRERR_NONE);
    reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    REQUIRE(dataset.gdalDataset()->SetSpatialRef(&reference) == CE_None);
    return dataset;
}

void check_bounds_near(const radix::tile::SrsBounds& actual, const radix::tile::SrsBounds& expected, double margin)
{
    CHECK(actual.min.x == Approx(expected.min.x).margin(margin));
    CHECK(actual.min.y == Approx(expected.min.y).margin(margin));
    CHECK(actual.max.x == Approx(expected.max.x).margin(margin));
    CHECK(actual.max.y == Approx(expected.max.y).margin(margin));
}

glm::dvec2 mercator(double longitude, double latitude)
{
    constexpr double radius = 6378137;
    return { radius * glm::radians(longitude), radius * std::log(std::tan(glm::radians(45 + latitude / 2))) };
}
} // namespace

TEST_CASE("dataset coverage is split at the antimeridian")
{
    const auto dataset = memory_raster({ 170, 0.625, 0, 5, 0, -0.3125 }, { 32, 32 }, 4326);

    const auto geographic = dataset.geographic_coverage();
    REQUIRE(geographic);
    REQUIRE(geographic->size() == 2);
    check_bounds_near((*geographic)[0], { { 170, -5 }, { 180, 5 } }, 1e-9);
    check_bounds_near((*geographic)[1], { { -180, -5 }, { -170, 5 } }, 1e-9);

    const auto mercator_bounds = dataset.mercator_coverage();
    REQUIRE(mercator_bounds);
    REQUIRE(mercator_bounds->size() == 2);
    check_bounds_near((*mercator_bounds)[0], { mercator(170, -5), mercator(180, 5) }, 1e-3);
    check_bounds_near((*mercator_bounds)[1], { mercator(-180, -5), mercator(-170, 5) }, 1e-3);
}

TEST_CASE("dataset coverage beyond the polar limit is empty in web mercator")
{
    const auto dataset = memory_raster({ 10, 0.1, 0, 89, 0, -0.02 }, { 32, 32 }, 4326);

    const auto geographic = dataset.geographic_coverage();
    REQUIRE(geographic);
    REQUIRE(geographic->size() == 1);
    check_bounds_near(geographic->front(), { { 10, 88.36 }, { 13.2, 89 } }, 1e-9);

    const auto mercator_bounds = dataset.mercator_coverage();
    REQUIRE(mercator_bounds);
    CHECK(mercator_bounds->empty());
}

TEST_CASE("web mercator dataset coverage is exact")
{
    // Tile (3, {4, 3}) in XYZ order.
    const double side = srs::webmercator_half_extent / 4;
    const auto dataset = memory_raster({ 0, side / 32, 0, side, 0, -side / 32 }, { 32, 32 }, 3857);

    const auto mercator_bounds = dataset.mercator_coverage();
    REQUIRE(mercator_bounds);
    REQUIRE(mercator_bounds->size() == 1);
    CHECK(mercator_bounds->front().min == glm::dvec2(0, 0));
    CHECK(mercator_bounds->front().max == glm::dvec2(side, side));
}

TEST_CASE("global dataset coverage is a single world rectangle")
{
    const auto dataset = memory_raster({ -180, 0.5, 0, 90, 0, -0.5 }, { 720, 360 }, 4326);

    const auto geographic = dataset.geographic_coverage();
    REQUIRE(geographic);
    REQUIRE(geographic->size() == 1);
    check_bounds_near(geographic->front(), { { -180, -90 }, { 180, 90 } }, 1e-9);

    const auto mercator_bounds = dataset.mercator_coverage();
    REQUIRE(mercator_bounds);
    REQUIRE(mercator_bounds->size() == 1);
    const double half = srs::webmercator_half_extent;
    check_bounds_near(mercator_bounds->front(), { { -half, -half }, { half, half } }, 1e-3);
}
