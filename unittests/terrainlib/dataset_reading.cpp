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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <numeric>
#include <numbers>
#include <string>
#include <tuple>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>
#include <gdal_priv.h>
#include "Dataset.h"
#include "DatasetReader.h"
#include "ctb/types.hpp"
#include "io/image.h"
#include "srs.h"

using namespace radix;

namespace {
// Writes the raster as a grayscale PNG, scaled from its minimum to its maximum.
template <typename T>
Expected<void> debug_out(const radix::Raster<T>& image, const std::filesystem::path& path)
{
    if (image.buffer().empty()) {
        return Error::fail(Error::Code::InvalidInput, "cannot write an empty debug raster", path);
    }
    const auto [min, max] = std::ranges::minmax(image);
    const auto range = float(max) - float(min);
    const auto gray = radix::raster::transform(image, [min, range](const auto value) {
        const auto intensity = range == 0.F ? std::uint8_t(0) : std::uint8_t(255.F * (float(value) - float(min)) / range);
        return glm::u8vec3(intensity);
    });
    return io::image::write(gray, path, { .overwrite = true });
}

void require_projection_available(const Dataset& dataset, const OGRSpatialReference& target_srs)
{
    const auto dataset_srs = dataset.srs().value();
    const auto source_bounds = dataset.bounds().value();

    std::shared_ptr<OGRCoordinateTransformation> transform;
    REQUIRE_NOTHROW(transform = srs::transformation(dataset_srs, target_srs).value());
    REQUIRE(transform != nullptr);

    std::array xs = { (source_bounds.min.x + source_bounds.max.x) / 2.0 };
    std::array ys = { (source_bounds.min.y + source_bounds.max.y) / 2.0 };
    REQUIRE(transform->Transform(static_cast<int>(xs.size()), xs.data(), ys.data()));
}
}

TEST_CASE("reading")
{
    const std::vector at100m = {
        "/austria/at_100m_mgi.tif",
        "/austria/at_100m_epsg3857.tif",
        "/austria/at_100m_epsg4326.tif",
        "/austria/at_x149m_y100m_epsg4326.tif",
    };

    const std::vector vienna20m = {
        "/austria/vienna_20m_mgi.tif",
        "/austria/vienna_20m_epsg3857.tif",
        "/austria/vienna_20m_epsg4326.tif",
    };

    const std::vector tauern10m = {
        "/austria/tauern_10m_mgi.tif",
        "/austria/tauern_10m_epsg3857.tif",
        "/austria/tauern_10m_epsg4326.tif",
    };

    const std::vector pizbuin1m = {
        "/austria/pizbuin_1m_mgi.tif",
        "/austria/pizbuin_1m_epsg3857.tif",
        "/austria/pizbuin_1m_epsg4326.tif",
    };

    OGRSpatialReference geographic_srs;
    geographic_srs.importFromEPSG(4326);
    geographic_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    SECTION("check for min and max heights")
    {
        const std::array test_projections = {
            std::make_pair(DatasetReader::Projection::Geographic, 4326),
            std::make_pair(DatasetReader::Projection::WebMercator, 3857),
        };
        const std::array test_locations = {
        // CRS bounds, [lower limit, at least one value smaller, at least one value larger, upper limit]
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
            std::make_tuple(
                "at100m",
                at100m,
                radix::tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}},
                std::make_tuple(100.0f, 120.0f, 3000.0f, 3800.0f)), // Austria is between 115 and 3798m
            std::make_tuple(
                "vienna20m",
                vienna20m,
                radix::tile::SrsBounds{{16.17, 48.14}, {16.59, 48.33}},
                std::make_tuple(140.0f, 180.0f, 500.0f, 560.0f)), // vienna, between 151 and 542m
#endif
            std::make_tuple(
                "tauern10m",
                tauern10m,
                radix::tile::SrsBounds{{12.6934117, 47.0739300}, {12.6944580, 47.0748649}},
                std::make_tuple(3700.0f, 3720.0f, 3790.0f, 3800.0f)), // Grossglockner, 3798m with some surroundings
        };

        for (const auto& test : test_locations) {
            const auto [test_name, test_datasets, geographic_bounds, limits] = test;

            for (std::string dataset_name : test_datasets) {
                const auto dataset = Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(dataset_name)));
                for (const auto& [test_projection, test_srs] : test_projections) {
                    OGRSpatialReference srs;
                    srs.importFromEPSG(test_srs);
                    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                    const auto srs_bounds = srs::non_exact_bounds_transform(geographic_bounds, geographic_srs, srs).value();

                    require_projection_available(*dataset, srs);
                    const DatasetReader reader(dataset, test_projection, 1);
                    REQUIRE(reader.projection() == test_projection);
                    if (ALP_UNITTESTS_DEBUG_IMAGES) {
                        const auto heights = reader.read(srs_bounds, 1000, 1000);
                        const auto s = std::string("/austria/").length();
                        const auto l = dataset_name.length() - std::string(".tif").length() - s;
                        const auto debug_file = fmt::format("./heights_{}_srs{}_{}.png", test_name, test_srs, dataset_name.substr(s, l));
                        REQUIRE(debug_out(heights, debug_file).has_value());
                    }

                    const auto heights = reader.read(srs_bounds, 100, 111);
                    REQUIRE(heights.width() == 100);
                    REQUIRE(heights.height() == 111);
                    const auto [lower_bound, lower_than, higher_than, higher_bound] = limits;
                    auto [min, max] = std::ranges::minmax(heights);
                    CHECK(min > lower_bound);
                    CHECK(min < lower_than);
                    CHECK(max < higher_bound);
                    CHECK(max > higher_than);
                }
            }
        }
    }

    SECTION("compare with ref render")
    {
        const std::array test_data = {
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
            std::make_tuple(
                "at100m",
                at100m,
                radix::tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}},
                620U, 350U, 42.0, 22.0),
#endif
            std::make_tuple(
                "pizbuin1m",
                pizbuin1m,
                radix::tile::SrsBounds{{10.105646780, 46.839864531}, {10.129815588, 46.847626067}},
                740U, 315U, 3.01, 0.006),
#if defined(ALP_UNITTESTS_EXTENDED) && ALP_UNITTESTS_EXTENDED
            std::make_tuple(
                "pizbuin1m_highres",
                pizbuin1m,
                radix::tile::SrsBounds{{10.105646780, 46.839864531}, {10.129815588, 46.847626067}},
                2000U, 850U, 10.0, 0.008),
#endif
        };

        for (const auto& test : test_data) {
            auto [test_name, datasets, ref_bounds, render_width, render_height, max_abs_diff, max_mse] = test;

            const auto ref_dataset = Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(datasets.front())));
            require_projection_available(*ref_dataset, geographic_srs);
            const auto ref_reader = DatasetReader(ref_dataset, DatasetReader::Projection::Geographic, 1);
            const auto ref_heights = ref_reader.read(ref_bounds, render_width, render_height);
            if (ALP_UNITTESTS_DEBUG_IMAGES) {
                REQUIRE(debug_out(ref_heights, fmt::format("./heights_ref.png")).has_value());
            }

            for (std::string dataset_name : datasets) {
                const auto dataset = Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(dataset_name)));
                require_projection_available(*dataset, geographic_srs);
                const auto reader = DatasetReader(dataset, DatasetReader::Projection::Geographic, 1);
                const auto heights = reader.read(ref_bounds, render_width, render_height);

                const auto s = std::string("/austria/").length();
                const auto l = dataset_name.length() - std::string(".tif").length() - s;

                if (ALP_UNITTESTS_DEBUG_IMAGES) {
                    REQUIRE(debug_out(ref_heights, fmt::format("./heights_{}_{}.png", test_name, dataset_name.substr(s, l))).has_value());

                    auto height_diffs = radix::Raster<float>({ render_width, render_height });
                    std::transform(ref_heights.begin(), ref_heights.end(), heights.begin(), height_diffs.begin(), [](auto a, auto b) { return std::abs(a - b); });
                    const auto path = fmt::format("./diffs_{}_{}.png", test_name, dataset_name.substr(s, l));
                    REQUIRE(debug_out(height_diffs, path).has_value());
                }
                auto largest_abs_diff = 0.0;
                const auto mse = std::transform_reduce(ref_heights.begin(), ref_heights.end(), heights.begin(), 0.0, std::plus<>(), [&largest_abs_diff](auto a, auto b) {
                    const auto t = std::abs(double(a) - double(b));
                    largest_abs_diff = std::max(t, largest_abs_diff);
                    return t * t;
                }) / double(ref_heights.buffer_length());
                //        fmt::print("{} | {};  mse: {}, largest_abs_diff: {}\n", test_name, dataset_name.substr(s, l), mse, largest_abs_diff);
                CHECK(largest_abs_diff < double(max_abs_diff));
                CHECK(mse < max_mse);
            }
        }
    }
}

TEST_CASE("min pixel size")
{
    using Projection = DatasetReader::Projection;
    const auto open = [](const char* name) { return Error::throwing_unwrap(Dataset::open_shared_raster(ALP_TEST_DATA_DIR + std::string(name))); };
    const auto pixel_size = [](const std::shared_ptr<Dataset>& dataset) {
        std::array<double, 6> geo_transform {};
        REQUIRE(dataset->gdalDataset()->GetGeoTransform(geo_transform.data()) == CE_None);
        return glm::dvec2(geo_transform[1], -geo_transform[5]);
    };
    const double metres_per_degree = 6378137.0 * std::numbers::pi / 180;
    // Within all Austrian test datasets.
    const radix::tile::SrsBounds geographic_bounds { { 12.0, 47.0 }, { 14.0, 48.0 } };
    const auto mercator_bounds = srs::non_exact_bounds_transform(geographic_bounds, srs::wgs84(), srs::webmercator()).value();

    SECTION("same srs as the dataset")
    {
        const auto mercator = open("/austria/at_100m_epsg3857.tif");
        const auto mercator_size = DatasetReader(mercator, Projection::WebMercator, 1).min_pixel_size(mercator_bounds);
        REQUIRE(mercator_size);
        CHECK(mercator_size->x == Catch::Approx(pixel_size(mercator).x));
        CHECK(mercator_size->y == Catch::Approx(pixel_size(mercator).y));

        const auto anisotropic = open("/austria/at_x149m_y100m_epsg4326.tif");
        const auto geographic_size = DatasetReader(anisotropic, Projection::Geographic, 1).min_pixel_size(geographic_bounds);
        REQUIRE(geographic_size);
        CHECK(geographic_size->x == Catch::Approx(pixel_size(anisotropic).x));
        CHECK(geographic_size->y == Catch::Approx(pixel_size(anisotropic).y));
    }

    SECTION("geographic dataset in web mercator")
    {
        // Web Mercator stretches y by 1 / cos(latitude), the minimum is at the southern edge.
        const auto dataset = open("/austria/at_100m_epsg4326.tif");
        const auto size = DatasetReader(dataset, Projection::WebMercator, 1).min_pixel_size(mercator_bounds);
        REQUIRE(size);
        const double south = geographic_bounds.min.y * std::numbers::pi / 180;
        CHECK(size->x == Catch::Approx(pixel_size(dataset).x * metres_per_degree).epsilon(1e-3));
        CHECK(size->y == Catch::Approx(pixel_size(dataset).y * metres_per_degree / std::cos(south)).epsilon(1e-3));
    }

    SECTION("bounds larger than the dataset are clipped to its coverage")
    {
        const auto dataset = open("/austria/at_100m_epsg4326.tif");
        const radix::tile::SrsBounds world { glm::dvec2(-srs::webmercator_half_extent), glm::dvec2(srs::webmercator_half_extent) };
        const auto size = DatasetReader(dataset, Projection::WebMercator, 1).min_pixel_size(world);
        REQUIRE(size);
        const double south = dataset->bounds().value().min.y * std::numbers::pi / 180;
        CHECK(size->y == Catch::Approx(pixel_size(dataset).y * metres_per_degree / std::cos(south)).epsilon(1e-3));
    }

    SECTION("projected dataset in web mercator")
    {
        // About 100 m on the ground, i.e. 1 / cos(47°) * 100 m in Web Mercator, reduced slightly by the grid rotation.
        const auto dataset = open("/austria/at_100m_mgi.tif");
        const auto size = DatasetReader(dataset, Projection::WebMercator, 1).min_pixel_size(mercator_bounds);
        REQUIRE(size);
        CHECK(size->x > 130);
        CHECK(size->x < 155);
        CHECK(size->y > 130);
        CHECK(size->y < 155);
    }

    SECTION("bounds outside the coverage")
    {
        const auto dataset = open("/austria/at_100m_epsg4326.tif");
        CHECK(!DatasetReader(dataset, Projection::Geographic, 1).min_pixel_size({ { 0.0, 0.0 }, { 1.0, 1.0 } }));
    }
}
