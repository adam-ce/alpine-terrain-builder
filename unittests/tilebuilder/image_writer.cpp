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
#include <filesystem>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <opencv2/imgcodecs.hpp>
#include <radix/raster.h>

#include "image_writer.h"

TEST_CASE("tile builder writes radix rasters as PNG images")
{
    const auto output_path = std::filesystem::temp_directory_path() / "alpine_terrain_builder_image_writer_test.png";
    std::filesystem::remove(output_path);

    radix::Raster<glm::u8vec3> raster({ 2, 2 });
    raster.pixel({ 0, 0 }) = { 255, 0, 0 };
    raster.pixel({ 1, 0 }) = { 0, 255, 0 };
    raster.pixel({ 0, 1 }) = { 0, 0, 255 };
    raster.pixel({ 1, 1 }) = { 255, 255, 255 };

    image::saveImageAsPng(raster, output_path.string());

    const auto decoded = cv::imread(output_path.string(), cv::IMREAD_COLOR);
    REQUIRE(decoded.rows == 2);
    REQUIRE(decoded.cols == 2);
    CHECK(decoded.at<cv::Vec3b>(0, 0) == cv::Vec3b(255, 0, 0));
    CHECK(decoded.at<cv::Vec3b>(0, 1) == cv::Vec3b(255, 255, 255));
    CHECK(decoded.at<cv::Vec3b>(1, 0) == cv::Vec3b(0, 0, 255));
    CHECK(decoded.at<cv::Vec3b>(1, 1) == cv::Vec3b(0, 255, 0));

    std::filesystem::remove(output_path);
}

TEST_CASE("tile builder handles image writer edge cases")
{
    SECTION("empty debug raster is rejected")
    {
        CHECK_THROWS_AS(image::debugOut(radix::Raster<float> {}, "empty.png"), std::invalid_argument);
    }

    SECTION("constant debug raster is written as black")
    {
        const auto output_path = std::filesystem::temp_directory_path() / "alpine_terrain_builder_constant_raster_test.png";
        std::filesystem::remove(output_path);

        radix::Raster<float> raster({ 2, 2 });
        std::ranges::fill(raster, 42.F);
        image::debugOut(raster, output_path.string());

        const auto decoded = cv::imread(output_path.string(), cv::IMREAD_COLOR);
        REQUIRE(decoded.rows == 2);
        REQUIRE(decoded.cols == 2);
        CHECK(cv::countNonZero(decoded.reshape(1)) == 0);

        std::filesystem::remove(output_path);
    }

    SECTION("PNG write failure is reported")
    {
        const auto missing_directory = std::filesystem::temp_directory_path() / "alpine_terrain_builder_missing_directory";
        std::filesystem::remove_all(missing_directory);
        const auto output_path = missing_directory / "image.png";

        CHECK_THROWS_AS(image::saveImageAsPng(radix::Raster<glm::u8vec3>({ 1, 1 }), output_path.string()), std::runtime_error);
    }
}
