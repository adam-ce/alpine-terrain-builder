#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "atlas/pull_reproject_texture.h"
#include "range_utils.h"

namespace {

cv::Mat make_texture(const cv::Vec3b top_left, const cv::Vec3b top_right) {
    cv::Mat texture(1, 2, CV_8UC3);
    texture.at<cv::Vec3b>(0, 0) = top_left;
    texture.at<cv::Vec3b>(0, 1) = top_right;
    return texture;
}

} // namespace

TEST_CASE("TextureReprojector copies pixels under an identity mapping", "[dag_builder][pull_reproject_texture]") {
    const cv::Mat source = make_texture(cv::Vec3b(10, 20, 30), cv::Vec3b(40, 50, 60));

    std::array<ReprojectionTriangle, 2> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[1].source_image_index = 0;
    triangles[1].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};
    triangles[1].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};

    TextureReprojector reprojector(glm::uvec2(2, 1), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30));
    CHECK(output.at<cv::Vec3b>(0, 1) == cv::Vec3b(40, 50, 60));
}

TEST_CASE("TextureReprojector linearly blends between neighboring source pixels", "[dag_builder][pull_reproject_texture]") {
    const cv::Mat source = make_texture(cv::Vec3b(0, 0, 0), cv::Vec3b(200, 200, 200));

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{1, cv::INTER_LINEAR});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    // The single output sample lands halfway between the two source pixels.
    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(100, 100, 100));
}

TEST_CASE("TextureReprojector leaves the background where triangles are degenerate", "[dag_builder][pull_reproject_texture]") {
    const cv::Mat source = make_texture(cv::Vec3b(10, 20, 30), cv::Vec3b(40, 50, 60));

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0.5, 0.5), glm::dvec2(0.5, 0.5), glm::dvec2(0.5, 0.5)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 0, 0));
}

TEST_CASE("TextureReprojector supersamples and averages partial pixel coverage", "[dag_builder][pull_reproject_texture]") {
    // Triangle covering only the lower-left sample at UV (0.25, 0.25)
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(100, 100, 100));

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    // Triangle covers lower-left quarter of output pixel
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(0.5, 0), glm::dvec2(0.5, 0.5)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{2, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    // Only 1 of 4 samples is covered, output is average of covered samples
    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(100, 100, 100));
}

TEST_CASE("TextureReprojector handles non-uniform coverage with gradient source", "[dag_builder][pull_reproject_texture]") {
    // 2x1 gradient source for testing coverage-aware averaging
    cv::Mat source(1, 2, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(50, 50, 50);
    source.at<cv::Vec3b>(0, 1) = cv::Vec3b(150, 150, 150);

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{2, cv::INTER_LINEAR});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    const auto pixel = output.at<cv::Vec3b>(0, 0);
    // With linear interpolation across gradient, should be intermediate value
    CHECK(pixel[0] > 50);
    CHECK(pixel[0] < 150);
}

TEST_CASE("TextureReprojector background stays zero when no samples covered", "[dag_builder][pull_reproject_texture]") {
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(255, 255, 255));

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    // Triangle outside output area
    triangles[0].target_uvs = {glm::dvec2(2, 2), glm::dvec2(3, 2), glm::dvec2(3, 3)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{2, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 0, 0));
}

TEST_CASE("TextureReprojector quad seam has no holes with supersampling", "[dag_builder][pull_reproject_texture]") {
    // Full coverage with two triangles forming a quad and supersampling
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(128, 128, 128));

    std::array<ReprojectionTriangle, 2> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[1].source_image_index = 0;
    triangles[1].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};
    triangles[1].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};

    TextureReprojector reprojector(glm::uvec2(4, 4), CV_8UC3, ReprojectionOptions{2, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    // Every output pixel must equal source color (no holes on seams)
    for (int y = 0; y < output.rows; y++) {
        for (int x = 0; x < output.cols; x++) {
            REQUIRE(output.at<cv::Vec3b>(y, x) == cv::Vec3b(128, 128, 128));
        }
    }
}

TEST_CASE("TextureReprojector renders identically regardless of winding order", "[dag_builder][pull_reproject_texture]") {
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(64, 64, 64));

    // CCW triangle
    std::array<ReprojectionTriangle, 1> triangles_ccw;
    triangles_ccw[0].source_image_index = 0;
    triangles_ccw[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles_ccw[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    // CW triangle (swap two vertices)
    std::array<ReprojectionTriangle, 1> triangles_cw;
    triangles_cw[0].source_image_index = 0;
    triangles_cw[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(1, 0)};
    triangles_cw[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(1, 0)};

    TextureReprojector reprojector(glm::uvec2(2, 2), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST});

    const cv::Mat output_ccw = reprojector.render(std::span(&source, 1), triangles_ccw);
    const cv::Mat output_cw = reprojector.render(std::span(&source, 1), triangles_cw);

    // Winding repair should make both renders identical
    for (int y = 0; y < output_ccw.rows; y++) {
        for (int x = 0; x < output_ccw.cols; x++) {
            CHECK(output_ccw.at<cv::Vec3b>(y, x) == output_cw.at<cv::Vec3b>(y, x));
        }
    }
}

TEST_CASE("TextureReprojector handles multiple source images with different colors", "[dag_builder][pull_reproject_texture]") {
    // Two 1x1 sources with different colors
    cv::Mat source0(1, 1, CV_8UC3, cv::Vec3b(255, 0, 0));    // red
    cv::Mat source1(1, 1, CV_8UC3, cv::Vec3b(0, 255, 0));    // green
    std::array<cv::Mat, 2> sources = {source0, source1};

    // Two triangles, each using a different source
    std::array<ReprojectionTriangle, 2> triangles;
    // Left triangle uses source 0 (red)
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(0.5, 0), glm::dvec2(0.5, 1)};
    // Right triangle uses source 1 (green)
    triangles[1].source_image_index = 1;
    triangles[1].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[1].target_uvs = {glm::dvec2(0.5, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    TextureReprojector reprojector(glm::uvec2(2, 1), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(sources, triangles);

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(255, 0, 0));    // red
    CHECK(output.at<cv::Vec3b>(0, 1) == cv::Vec3b(0, 255, 0));    // green
}

TEST_CASE("TextureReprojector clamps UVs and applies edge replication", "[dag_builder][pull_reproject_texture]") {
    cv::Mat source(2, 2, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);      // red
    source.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);      // green
    source.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 0, 255);      // blue
    source.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 255, 0);    // yellow

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    // All source UVs lie outside [0, 1] and get clamped to the (1, 1) corner.
    triangles[0].source_uvs = {glm::dvec2(2, 2), glm::dvec2(3, 2), glm::dvec2(3, 3)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    // Every sample reads the bottom-right border texel (yellow).
    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(255, 255, 0));
}

TEST_CASE("TextureReprojector pins rounding behavior for linear interpolation blends", "[dag_builder][pull_reproject_texture]") {
    // Create a 2x1 source with black and white to test 127.5 blend
    cv::Mat source(1, 2, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 0);        // black
    source.at<cv::Vec3b>(0, 1) = cv::Vec3b(255, 255, 255);  // white

    std::array<ReprojectionTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    TextureReprojector reprojector(glm::uvec2(1, 1), CV_8UC3, ReprojectionOptions{1, cv::INTER_LINEAR});
    const cv::Mat output = reprojector.render(std::span(&source, 1), triangles);

    const auto pixel = output.at<cv::Vec3b>(0, 0);
    // Linear interpolation at center between 0 and 255 produces 127.5
    // OpenCV cvRound uses round-half-to-even: 127.5 rounds to 128
    CHECK(pixel[0] == 128);
    CHECK(pixel[1] == 128);
    CHECK(pixel[2] == 128);
}

namespace {

// A quad over the middle ninth of the target, which holds only the centre sample of a 3x3 output.
std::array<ReprojectionTriangle, 2> make_centre_quad() {
    const glm::dvec2 low(0.34);
    const glm::dvec2 high(0.66);

    std::array<ReprojectionTriangle, 2> triangles;
    triangles[0].source_uvs = {glm::dvec2(0.5), glm::dvec2(0.5), glm::dvec2(0.5)};
    triangles[0].target_uvs = {low, glm::dvec2(high.x, low.y), high};
    triangles[1].source_uvs = triangles[0].source_uvs;
    triangles[1].target_uvs = {low, high, glm::dvec2(low.x, high.y)};
    return triangles;
}

} // namespace

TEST_CASE("TextureReprojector extends the covered region by the requested gutter", "[dag_builder][pull_reproject_texture]") {
    const cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(10, 20, 30));
    const std::array<ReprojectionTriangle, 2> triangles = make_centre_quad();

    TextureReprojector without_gutter(glm::uvec2(3, 3), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST, 0});
    const cv::Mat unfilled = without_gutter.render(std::span(&source, 1), triangles);

    CHECK(unfilled.at<cv::Vec3b>(1, 1) == cv::Vec3b(10, 20, 30));
    CHECK(unfilled.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 0, 0));

    TextureReprojector with_gutter(glm::uvec2(3, 3), CV_8UC3, ReprojectionOptions{1, cv::INTER_NEAREST, 1});
    const cv::Mat filled = with_gutter.render(std::span(&source, 1), triangles);

    for (const uint32_t y : range(uint32_t{3})) {
        for (const uint32_t x : range(uint32_t{3})) {
            REQUIRE(filled.at<cv::Vec3b>(y, x) == cv::Vec3b(10, 20, 30));
        }
    }
}
