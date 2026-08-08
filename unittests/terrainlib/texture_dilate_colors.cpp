#include <cstdint>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "range_utils.h"
#include "texture/dilate_colors.h"

namespace {

// An image and the coverage it is dilated against, so the fixtures read as one grid.
struct Input {
    cv::Mat image;
    texture::Mask coverage;
};

Input make_input(const glm::uvec2 size, const int type = CV_8UC1) {
    return {
        cv::Mat(size.y, size.x, type, cv::Scalar::all(0)),
        texture::Mask(size)};
}

template <typename Pixel>
void cover(Input &input, const glm::uvec2 at, const Pixel value) {
    input.image.at<Pixel>(at.y, at.x) = value;
    input.coverage.set(at.x, at.y);
}

} // namespace

TEST_CASE("dilate_colors leaves everything alone for a zero radius", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5));
    cover<uint8_t>(input, glm::uvec2(2), 100);
    const cv::Mat image_before = input.image.clone();

    texture::dilate_colors_inplace(input.image, input.coverage, 0);

    CHECK(mat_equals(input.image, image_before));
    CHECK(input.coverage.count() == 1);
}

TEST_CASE("dilate_colors grows the covered region by one ring per round", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5));
    cover<uint8_t>(input, glm::uvec2(2), 100);

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    // The eight neighbours take the centre's color, and nothing beyond them is touched.
    for (const uint32_t y : range(uint32_t{5})) {
        for (const uint32_t x : range(uint32_t{5})) {
            const glm::ivec2 offset = glm::ivec2(x, y) - glm::ivec2(2);
            const bool in_ring = glm::all(glm::lessThanEqual(glm::abs(offset), glm::ivec2(1)));
            REQUIRE(input.coverage.get(x, y) == in_ring);
            REQUIRE(input.image.at<uint8_t>(y, x) == (in_ring ? 100 : 0));
        }
    }
}

TEST_CASE("dilate_colors reaches the whole image given enough rounds", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5));
    cover<uint8_t>(input, glm::uvec2(2), 100);

    texture::dilate_colors_inplace(input.image, input.coverage, 2);

    for (const uint32_t y : range(uint32_t{5})) {
        for (const uint32_t x : range(uint32_t{5})) {
            REQUIRE(input.coverage.get(x, y));
            REQUIRE(input.image.at<uint8_t>(y, x) == 100);
        }
    }
}

TEST_CASE("dilate_colors averages only the covered neighbours", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 40);
    cover<uint8_t>(input, glm::uvec2(2, 0), 200);

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    CHECK(input.image.at<uint8_t>(0, 1) == 120);
}

TEST_CASE("dilate_colors has nothing to spread when nothing is covered", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(4));
    input.image.setTo(cv::Scalar::all(70));
    const cv::Mat image_before = input.image.clone();

    texture::dilate_colors_inplace(input.image, input.coverage, 3);

    CHECK(mat_equals(input.image, image_before));
    CHECK(input.coverage.count() == 0);
}

TEST_CASE("dilate_colors leaves a fully covered image alone", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3));
    for (const uint32_t y : range(uint32_t{3})) {
        for (const uint32_t x : range(uint32_t{3})) {
            cover<uint8_t>(input, glm::uvec2(x, y), uint8_t(10 * (3 * y + x)));
        }
    }
    const cv::Mat image_before = input.image.clone();

    texture::dilate_colors_inplace(input.image, input.coverage, 2);

    CHECK(mat_equals(input.image, image_before));
}

TEST_CASE("dilate_colors spreads every channel", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3), CV_8UC3);
    cover<cv::Vec3b>(input, glm::uvec2(1), cv::Vec3b(10, 20, 30));

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    CHECK(input.image.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30));
    CHECK(input.image.at<cv::Vec3b>(2, 2) == cv::Vec3b(10, 20, 30));
}

TEST_CASE("dilate_colors dispatches on the image depth", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3), CV_32FC1);
    cover<float>(input, glm::uvec2(1), 1.5f);

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    CHECK(input.image.at<float>(0, 0) == Catch::Approx(1.5));
}

TEST_CASE("dilate_colors rejects a coverage that does not match the image", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3));
    texture::Mask wrong_size(glm::uvec2(4));

    CHECK_THROWS(texture::dilate_colors_inplace(input.image, wrong_size, 1));
}

TEST_CASE("dilate_colors leaves the input alone when returning a copy", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3));
    cover<uint8_t>(input, glm::uvec2(1), 100);
    const cv::Mat image_before = input.image.clone();

    const cv::Mat dilated = texture::dilate_colors(input.image, input.coverage, 1);

    CHECK(mat_equals(input.image, image_before));
    CHECK(input.coverage.count() == 1);
    CHECK(dilated.at<uint8_t>(0, 0) == 100);
}