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

#include <array>
#include <numeric>
#include <string>
#include <tuple>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>
#include "Dataset.h"
#include "DatasetReader.h"
#include "ctb/types.hpp"
#include "srs.h"

using namespace radix;

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

    OGRSpatialReference geodetic_srs;
    geodetic_srs.importFromEPSG(4326);
    geodetic_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    SECTION("check for min and max heights")
    {
        const std::array test_srses = { 4326, 3857 };
        const std::array test_locations = {
        // CRS bounds, [lower limit, at least one value smaller, at least one value larger, upper limit]
#if defined(ATB_UNITTESTS_EXTENDED) && ATB_UNITTESTS_EXTENDED
            std::make_tuple(
                "at100m",
                at100m,
                tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}},
                std::make_tuple(100.0f, 120.0f, 3000.0f, 3800.0f)), // Austria is between 115 and 3798m
            std::make_tuple(
                "vienna20m",
                vienna20m,
                tile::SrsBounds{{16.17, 48.14}, {16.59, 48.33}},
                std::make_tuple(140.0f, 180.0f, 500.0f, 560.0f)), // vienna, between 151 and 542m
#endif
            std::make_tuple(
                "tauern10m",
                tauern10m,
                tile::SrsBounds{{12.6934117, 47.0739300}, {12.6944580, 47.0748649}},
                std::make_tuple(3700.0f, 3720.0f, 3790.0f, 3800.0f)), // Grossglockner, 3798m with some surroundings
        };

        for (const auto& test : test_locations) {
            const auto [test_name, test_datasets, geodetic_bounds, limits] = test;

            for (std::string dataset_name : test_datasets) {
                const auto dataset = Dataset::make_shared(ATB_TEST_DATA_DIR + std::string(dataset_name));
                for (const auto& test_srs : test_srses) {
                    OGRSpatialReference srs;
                    srs.importFromEPSG(test_srs);
                    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                    const auto srs_bounds = srs::non_exact_bounds_transform(geodetic_bounds, geodetic_srs, srs);

                    const auto reader = DatasetReader(dataset, srs, 1);
                    if (ATB_UNITTESTS_DEBUG_IMAGES) {
                        const auto heights = reader.read(srs_bounds, 1000, 1000);
                        const auto s = std::string("/austria/").length();
                        const auto l = dataset_name.length() - std::string(".tif").length() - s;
                        const auto debug_file = fmt::format("./heights_{}_srs{}_{}.png", test_name, test_srs, dataset_name.substr(s, l));
                        image::debugOut(heights, debug_file);
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
#if defined(ATB_UNITTESTS_EXTENDED) && ATB_UNITTESTS_EXTENDED
            std::make_tuple(
                "at100m",
                at100m,
                tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}},
                620U, 350U, 42.0, 22.0),
#endif
            std::make_tuple(
                "pizbuin1m",
                pizbuin1m,
                tile::SrsBounds{{10.105646780, 46.839864531}, {10.129815588, 46.847626067}},
                740U, 315U, 3.01, 0.006),
#if defined(ATB_UNITTESTS_EXTENDED) && ATB_UNITTESTS_EXTENDED
            std::make_tuple(
                "pizbuin1m_highres",
                pizbuin1m,
                tile::SrsBounds{{10.105646780, 46.839864531}, {10.129815588, 46.847626067}},
                2000U, 850U, 10.0, 0.008),
#endif
        };

        for (const auto& test : test_data) {
            auto [test_name, datasets, ref_bounds, render_width, render_height, max_abs_diff, max_mse] = test;

            const auto ref_dataset = Dataset::make_shared(ATB_TEST_DATA_DIR + std::string(datasets.front()));
            const auto ref_reader = DatasetReader(ref_dataset, geodetic_srs, 1);
            const auto ref_heights = ref_reader.read(ref_bounds, render_width, render_height);
            if (ATB_UNITTESTS_DEBUG_IMAGES)
                image::debugOut(ref_heights, fmt::format("./heights_ref.png"));

            for (std::string dataset_name : datasets) {
                const auto dataset = Dataset::make_shared(ATB_TEST_DATA_DIR + std::string(dataset_name));
                const auto reader = DatasetReader(dataset, geodetic_srs, 1);
                const auto heights = reader.read(ref_bounds, render_width, render_height);

                const auto s = std::string("/austria/").length();
                const auto l = dataset_name.length() - std::string(".tif").length() - s;

                if (ATB_UNITTESTS_DEBUG_IMAGES) {
                    image::debugOut(ref_heights, fmt::format("./heights_{}_{}.png", test_name, dataset_name.substr(s, l)));

                    auto height_diffs = HeightData(render_width, render_height);
                    std::transform(ref_heights.begin(), ref_heights.end(), heights.begin(), height_diffs.begin(), [](auto a, auto b) { return std::abs(a - b); });
                    const auto path = fmt::format("./diffs_{}_{}.png", test_name, dataset_name.substr(s, l));
                    image::debugOut(height_diffs, path);
                }
                auto largest_abs_diff = 0.0;
                const auto mse = std::transform_reduce(ref_heights.begin(), ref_heights.end(), heights.begin(), 0.0, std::plus<>(), [&largest_abs_diff](auto a, auto b) {
                    const auto t = std::abs(double(a) - double(b));
                    largest_abs_diff = std::max(t, largest_abs_diff);
                    return t * t;
                }) / double(ref_heights.size());
                //        fmt::print("{} | {};  mse: {}, largest_abs_diff: {}\n", test_name, dataset_name.substr(s, l), mse, largest_abs_diff);
                CHECK(largest_abs_diff < double(max_abs_diff));
                CHECK(mse < max_mse);
            }
        }
    }

#if defined(ATB_UNITTESTS_EXTENDED) && ATB_UNITTESTS_EXTENDED
    SECTION("overview without warping")
    {
        REQUIRE(std::string(ATB_UNITTESTS_AUSTRIA_HIGHRES).length() > 5);
        const auto border = 5;
        const auto low_res_ds = Dataset::make_shared(ATB_TEST_DATA_DIR "/austria/at_100m_mgi.tif");
        const auto high_res_ds = Dataset::make_shared(ATB_UNITTESTS_AUSTRIA_HIGHRES);
        const auto srs = low_res_ds->srs();

        const auto low_res_reader = DatasetReader(low_res_ds, srs, 1);
        const auto high_res_reader = DatasetReader(high_res_ds, srs, 1);
        const auto render_width = low_res_ds->widthInPixels() - 2 * border;
        const auto render_height = low_res_ds->heightInPixels() - 2 * border;

        // the error is not 0, apparently there is still some resampling going on. the diff image at least looks like sampling artifacts.
        // in theory, gdalwarp should do the exact same thing as this unit test, but for some reason this is not the case. That's also
        // indicated by the errors on the borders (which we are cutting away).
        // i'm ignoring it for now, as the error seems low enouugh.
        const auto max_abs_diff = 18.0;
        const auto max_mse = 0.6;

        const auto pixel_width = low_res_ds->pixelWidthIn(srs);
        const auto pixel_height = low_res_ds->pixelHeightIn(srs);
        auto srs_bounds = srs::nonExactBoundsTransform(low_res_ds->bounds(), low_res_ds->srs(), srs);
        srs_bounds.min = { srs_bounds.min.x + border * pixel_width, srs_bounds.min.y + border * pixel_height };
        srs_bounds.max = { srs_bounds.max.x - border * pixel_width, srs_bounds.max.y - border * pixel_height };

        const auto t0 = std::chrono::high_resolution_clock::now();
        const auto low_res_heights = low_res_reader.read(srs_bounds, render_width, render_height);
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto high_res_heights = high_res_reader.readWithOverviews(srs_bounds, render_width, render_height);
        const auto t2 = std::chrono::high_resolution_clock::now();

        REQUIRE(low_res_heights.width() == render_width);
        REQUIRE(low_res_heights.height() == render_height);
        REQUIRE(high_res_heights.width() == render_width);
        REQUIRE(high_res_heights.height() == render_height);

        const auto low_res_time = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        const auto high_res_time = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        fmt::print("low res time: {}; high res time: {}\n", double(low_res_time) / 1000.0, double(high_res_time) / 1000.0);

        if (ATB_UNITTESTS_DEBUG_IMAGES) {
            image::debugOut(low_res_heights, fmt::format("./low_res_heights.png"));
            image::debugOut(high_res_heights, fmt::format("./high_res_heights.png"));

            auto height_diffs = HeightData(render_width, render_height);
            std::transform(low_res_heights.begin(), low_res_heights.end(), high_res_heights.begin(), height_diffs.begin(), [](auto a, auto b) { return std::abs(a - b); });
            image::debugOut(height_diffs, "./diff_low_res_high_res.png");
        }
        auto largest_abs_diff = 0.0;
        const auto mse = std::transform_reduce(low_res_heights.begin(), low_res_heights.end(), high_res_heights.begin(), 0.0, std::plus<>(), [&largest_abs_diff](auto a, auto b) {
            const auto t = std::abs(double(a) - double(b));
            largest_abs_diff = std::max(t, largest_abs_diff);
            return t * t;
        }) / double(low_res_heights.size());
        //    fmt::print("mse: {}, largest_abs_diff: {}\n", mse, largest_abs_diff);
        CHECK(largest_abs_diff < double(max_abs_diff));
        CHECK(mse < max_mse);
    }

    SECTION("overview with warping")
    {
        REQUIRE(std::string(ATB_UNITTESTS_AUSTRIA_HIGHRES).length() > 5);
        const auto low_res_ds = Dataset::make_shared(ATB_TEST_DATA_DIR "/austria/at_100m_epsg4326.tif");
        const auto high_res_ds = Dataset::make_shared(ATB_UNITTESTS_AUSTRIA_HIGHRES);
        const auto srs = low_res_ds->srs();
        const auto low_res_reader = DatasetReader(low_res_ds, srs, 1);
        const auto high_res_reader = DatasetReader(high_res_ds, srs, 1);
        const auto srs_bounds = srs::nonExactBoundsTransform(tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}}, geodetic_srs, srs);

        const auto render_width = unsigned(low_res_ds->widthInPixels(srs_bounds, srs));
        const auto render_height = unsigned(low_res_ds->heightInPixels(srs_bounds, srs));
        const auto max_abs_diff = 70.0;
        const auto max_mse = 8.0;

        const auto low_res_heights = low_res_reader.read(srs_bounds, render_width, render_height);
        const auto high_res_heights = high_res_reader.readWithOverviews(srs_bounds, render_width, render_height);

        REQUIRE(low_res_heights.width() == render_width);
        REQUIRE(low_res_heights.height() == render_height);
        REQUIRE(high_res_heights.width() == render_width);
        REQUIRE(high_res_heights.height() == render_height);

        if (ATB_UNITTESTS_DEBUG_IMAGES) {
            image::debugOut(low_res_heights, fmt::format("./ov_with_warping_low_res_heights.png"));
            image::debugOut(high_res_heights, fmt::format("./ov_with_warping_high_res_heights.png"));

            auto height_diffs = HeightData(render_width, render_height);
            std::transform(low_res_heights.begin(), low_res_heights.end(), high_res_heights.begin(), height_diffs.begin(), [](auto a, auto b) { return std::abs(a - b); });
            image::debugOut(height_diffs, "./ov_with_warping_diff_low_res_high_res.png");
        }
        auto largest_abs_diff = 0.0;
        const auto mse = std::transform_reduce(low_res_heights.begin(), low_res_heights.end(), high_res_heights.begin(), 0.0, std::plus<>(), [&largest_abs_diff](auto a, auto b) {
            const auto t = std::abs(double(a) - double(b));
            largest_abs_diff = std::max(t, largest_abs_diff);
            return t * t;
        }) / double(low_res_heights.size());
        //    fmt::print("mse: {}, largest_abs_diff: {}\n", mse, largest_abs_diff);
        CHECK(largest_abs_diff < double(max_abs_diff));
        CHECK(mse < max_mse);
    }

    SECTION("lowres overview with warping")
    {
        REQUIRE(std::string(ATB_UNITTESTS_AUSTRIA_HIGHRES).length() > 5);
        const auto low_res_ds = Dataset::make_shared(ATB_TEST_DATA_DIR "/austria/at_100m_epsg4326.tif");
        const auto high_res_ds = Dataset::make_shared(ATB_UNITTESTS_AUSTRIA_HIGHRES);
        const auto srs = low_res_ds->srs();
        const auto low_res_reader = DatasetReader(low_res_ds, srs, 1);
        const auto high_res_reader = DatasetReader(high_res_ds, srs, 1);
        const auto srs_bounds = srs::nonExactBoundsTransform(tile::SrsBounds{{9.5, 46.4}, {17.1, 49.0}}, geodetic_srs, srs);

        const auto render_width = unsigned(low_res_ds->widthInPixels(srs_bounds, srs)) / 10;
        const auto render_height = unsigned(low_res_ds->heightInPixels(srs_bounds, srs)) / 10;
        const auto max_abs_diff = 170;
        const auto max_mse = 300.0;

        const auto low_res_heights = low_res_reader.read(srs_bounds, render_width, render_height);
        const auto high_res_heights = high_res_reader.readWithOverviews(srs_bounds, render_width, render_height);

        REQUIRE(low_res_heights.width() == render_width);
        REQUIRE(low_res_heights.height() == render_height);
        REQUIRE(high_res_heights.width() == render_width);
        REQUIRE(high_res_heights.height() == render_height);

        if (ATB_UNITTESTS_DEBUG_IMAGES) {
            image::debugOut(low_res_heights, fmt::format("./lowres_ov_with_warping_low_res_heights.png"));
            image::debugOut(high_res_heights, fmt::format("./lowres_ov_with_warping_high_res_heights.png"));

            auto height_diffs = HeightData(render_width, render_height);
            std::transform(low_res_heights.begin(), low_res_heights.end(), high_res_heights.begin(), height_diffs.begin(), [](auto a, auto b) { return std::abs(a - b); });
            image::debugOut(height_diffs, "./lowres_ov_with_warping_diff_low_res_high_res.png");
        }
        auto largest_abs_diff = 0.0;
        const auto mse = std::transform_reduce(low_res_heights.begin(), low_res_heights.end(), high_res_heights.begin(), 0.0, std::plus<>(), [&largest_abs_diff](auto a, auto b) {
            const auto t = std::abs(double(a) - double(b));
            largest_abs_diff = std::max(t, largest_abs_diff);
            return t * t;
        }) / double(low_res_heights.size());
        //    fmt::print("render w/h: {}/{}, mse: {}, largest_abs_diff: {}\n", render_width, render_height, mse, largest_abs_diff);
        CHECK(largest_abs_diff < double(max_abs_diff));
        CHECK(mse < max_mse);
    }
#endif
}
