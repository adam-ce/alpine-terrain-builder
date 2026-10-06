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

#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <gdal_priv.h>
#include <glm/glm.hpp>

#include "../temporary_directory.h"
#include "Dataset.h"
#include "mesh/io.h"
#include "mesh_builder.h"
#include "srs.h"
#include "terrainbuilder.h"

using test::TemporaryDirectory;

namespace {

// Writes a north-up raster with a constant height of 100 m.
std::filesystem::path write_raster(
    const std::filesystem::path& directory, const OGRSpatialReference& srs, const radix::tile::SrsBounds& bounds, const glm::uvec2 size)
{
    GDALAllRegister();
    const auto path = directory / "heights.tif";
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    REQUIRE(driver != nullptr);
    GDALDataset* dataset = driver->Create(path.c_str(), int(size.x), int(size.y), 1, GDT_Float32, nullptr);
    REQUIRE(dataset != nullptr);
    std::array<double, 6> transform { bounds.min.x, bounds.width() / size.x, 0, bounds.max.y, 0, -bounds.height() / size.y };
    REQUIRE(dataset->SetGeoTransform(transform.data()) == CE_None);
    REQUIRE(dataset->SetSpatialRef(&srs) == CE_None);
    GDALRasterBand* band = dataset->GetRasterBand(1);
    REQUIRE(band->SetNoDataValue(-9999) == CE_None);
    REQUIRE(band->SetUnitType("m") == CE_None);
    std::vector<float> heights(std::size_t(size.x) * size.y, 100.0f);
    REQUIRE(band->RasterIO(GF_Write, 0, 0, int(size.x), int(size.y), heights.data(), int(size.x), int(size.y), GDT_Float32, 0, 0) == CE_None);
    double min = 0, max = 0, mean = 0, deviation = 0;
    REQUIRE(band->ComputeStatistics(false, &min, &max, &mean, &deviation, nullptr, nullptr) == CE_None);
    GDALClose(dataset);
    return path;
}

// ECEF bounds of the given longitude/latitude bounds in degrees, between heights 0 and 200 m.
radix::geometry::Aabb3d ecef_bounds(const glm::dvec2& longitudes, const glm::dvec2& latitudes)
{
    return srs::ecef_coverage(srs::wgs84(), { { longitudes.x, latitudes.x, 0 }, { longitudes.y, latitudes.y, 200 } }).value();
}

double longitude(const glm::dvec3& ecef_position) { return glm::degrees(std::atan2(ecef_position.y, ecef_position.x)); }

} // namespace

TEST_CASE("native read windows follow the longitudes of the dataset", "[terrainbuilder][coverage]")
{
    const auto wgs84 = srs::wgs84();
    const auto near_minus_175 = ecef_bounds({ -175.1, -174.9 }, { -0.1, 0.1 });

    SECTION("dataset stored at [0, 360]")
    {
        const auto windows = terrainbuilder::native_read_windows(wgs84, { { 0, -10 }, { 360, 10 } }, near_minus_175).value();
        REQUIRE(windows.size() == 1);
        CHECK(windows[0].min.x > 184.8);
        CHECK(windows[0].max.x < 185.2);
    }

    SECTION("dataset stored at [180, 190]")
    {
        const radix::tile::SrsBounds native_bounds { { 180, -5 }, { 190, 5 } };
        const auto windows = terrainbuilder::native_read_windows(wgs84, native_bounds, near_minus_175).value();
        REQUIRE(windows.size() == 1);
        CHECK(windows[0].min.x > 184.8);
        CHECK(windows[0].max.x < 185.2);
        CHECK(terrainbuilder::native_read_windows(wgs84, native_bounds, ecef_bounds({ 174.9, 175.1 }, { -0.1, 0.1 })).value().empty());
    }

    SECTION("bounds across the antimeridian are read on both edges of the dataset")
    {
        const auto windows = terrainbuilder::native_read_windows(wgs84, { { -180, -10 }, { 180, 10 } }, ecef_bounds({ 179.9, 180.1 }, { -0.1, 0.1 })).value();
        REQUIRE(windows.size() == 2);
        const auto& east = windows[0].max.x == 180 ? windows[0] : windows[1];
        const auto& west = windows[0].max.x == 180 ? windows[1] : windows[0];
        CHECK(east.max.x == 180);
        CHECK(east.min.x > 179.8);
        CHECK(west.min.x == -180);
        CHECK(west.max.x < -179.8);
    }

    SECTION("projected datasets are not shifted")
    {
        const auto austria_lambert = srs::from_epsg(31287).value();
        const radix::tile::SrsBounds native_bounds { { 100000, 250000 }, { 700000, 600000 } };
        const glm::dvec3 vienna = srs::transform_point(wgs84, austria_lambert, glm::dvec3(16.37, 48.2, 0)).value();
        const auto windows = terrainbuilder::native_read_windows(austria_lambert, native_bounds, ecef_bounds({ 16.36, 16.38 }, { 48.19, 48.21 })).value();
        REQUIRE(windows.size() == 1);
        CHECK(windows[0].contains(glm::dvec2(vienna)));
        CHECK(windows[0].width() < 3000);
    }
}

TEST_CASE("reference meshes are built across longitude seams", "[terrainbuilder][coverage]")
{
    TemporaryDirectory directory("sf-coverage");
    const auto wgs84 = srs::wgs84();
    const auto ecef = srs::ecef();
    const auto webmercator = srs::webmercator();
    radix::tile::SrsBounds texture_bounds;

    SECTION("dataset stored at [180, 190]")
    {
        Dataset dataset(write_raster(directory.path(), wgs84, { { 180, -5 }, { 190, 5 } }, { 1000, 1000 }));
        const auto mesh
            = terrainbuilder::build_reference_mesh_patch(dataset, ecef, ecef, ecef_bounds({ -175.2, -174.8 }, { -0.2, 0.2 }), webmercator, texture_bounds);
        REQUIRE(mesh.has_value());
        CHECK(mesh->face_count() > 100);
        for (const glm::dvec3& position : mesh->positions) {
            REQUIRE(longitude(position) > -175.25);
            REQUIRE(longitude(position) < -174.75);
        }
    }

    SECTION("dataset across the antimeridian")
    {
        Dataset dataset(write_raster(directory.path(), wgs84, { { -180, -5 }, { 180, 5 } }, { 3600, 100 }));
        const auto mesh
            = terrainbuilder::build_reference_mesh_patch(dataset, ecef, ecef, ecef_bounds({ 179.5, 180.5 }, { -0.5, 0.5 }), webmercator, texture_bounds);
        REQUIRE(mesh.has_value());
        std::size_t east = 0;
        std::size_t west = 0;
        for (const glm::dvec3& position : mesh->positions) {
            REQUIRE(std::abs(longitude(position)) > 179.4);
            (longitude(position) > 0 ? east : west)++;
        }
        CHECK(east > 10);
        CHECK(west > 10);
    }
}

TEST_CASE("batch builds datasets crossing the prime meridian", "[terrainbuilder][coverage]")
{
    TemporaryDirectory directory("sf-coverage-batch");
    const auto wgs84 = srs::wgs84();
    const auto ecef = srs::ecef();
    const auto webmercator = srs::webmercator();
    // Crosses the plane between two level-one nodes, so traversal starts at the root.
    Dataset dataset(write_raster(directory.path(), wgs84, { { -0.02, 51.47 }, { 0.02, 51.49 } }, { 400, 200 }));
    const auto output = directory.path() / "output";

    SECTION("target levels below the minimum are rejected")
    {
        CHECK_FALSE(terrainbuilder::build_all_patches(dataset, 1, webmercator, nullptr, ecef, output, ".sfmesh", false).has_value());
        CHECK_FALSE(std::filesystem::exists(output));
    }

    SECTION("nodes on both sides are built")
    {
        REQUIRE(terrainbuilder::build_all_patches(dataset, 12, webmercator, nullptr, ecef, output, ".sfmesh", false).has_value());
        std::size_t east = 0;
        std::size_t west = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(output)) {
            if (entry.path().extension() != ".sfmesh") {
                continue;
            }
            const auto mesh = mesh::io::load_from_path(entry.path());
            REQUIRE(mesh.has_value());
            for (const glm::dvec3& position : mesh->positions) {
                (position.y > 0 ? east : west)++;
            }
        }
        CHECK(east > 1000);
        CHECK(west > 1000);
    }
}
