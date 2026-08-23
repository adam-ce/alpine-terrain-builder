#include "../catch2_helpers.h"

#include "io/conversion.h"

#include <array>
#include <cstdint>

namespace {

template <typename Pixel>
void check_round_trip(const int cv_type)
{
    cv::Mat source(2, 3, cv_type);
    auto bytes = std::span<std::uint8_t>(source.ptr<std::uint8_t>(), source.total() * source.elemSize());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(index + 1);
    }

    const auto raster = io::conversion::to_raster<Pixel>(source);
    REQUIRE(raster.has_value());
    CHECK(raster->size() == glm::uvec2(3, 2));

    const auto result = io::conversion::to_mat(*raster);
    REQUIRE(result.has_value());
    CHECK(result->type() == source.type());
    CHECK(result->rows == source.rows);
    CHECK(result->cols == source.cols);
    CHECK(std::equal(bytes.begin(), bytes.end(), result->template ptr<std::uint8_t>()));
}

} // namespace

TEST_CASE("io::conversion round trips supported scalar and vector pixels")
{
    check_round_trip<std::uint8_t>(CV_8UC1);
    check_round_trip<std::int8_t>(CV_8SC1);
    check_round_trip<std::uint16_t>(CV_16UC1);
    check_round_trip<std::int16_t>(CV_16SC1);
    check_round_trip<std::int32_t>(CV_32SC1);
    check_round_trip<float>(CV_32FC1);
    check_round_trip<double>(CV_64FC1);
    check_round_trip<glm::u8vec2>(CV_8UC2);
    check_round_trip<glm::u8vec3>(CV_8UC3);
    check_round_trip<glm::u8vec4>(CV_8UC4);
    check_round_trip<glm::vec3>(CV_32FC3);
    check_round_trip<glm::dvec4>(CV_64FC4);
}

TEST_CASE("io::conversion copies non-contiguous matrices row by row")
{
    cv::Mat backing(4, 6, CV_8UC3);
    for (int row = 0; row < backing.rows; ++row) {
        for (int column = 0; column < backing.cols; ++column) {
            backing.at<cv::Vec3b>(row, column)
                = cv::Vec3b(static_cast<std::uint8_t>(row), static_cast<std::uint8_t>(column), static_cast<std::uint8_t>(row * backing.cols + column));
        }
    }
    const cv::Mat source = backing(cv::Rect(1, 1, 3, 2));
    REQUIRE_FALSE(source.isContinuous());

    auto raster = io::conversion::to_raster<glm::u8vec3>(source);
    REQUIRE(raster.has_value());
    CHECK(raster->pixel({ 0, 0 }) == glm::u8vec3(1, 1, 7));
    CHECK(raster->pixel({ 2, 1 }) == glm::u8vec3(2, 3, 15));

    backing.setTo(cv::Scalar::all(0));
    CHECK(raster->pixel({ 0, 0 }) == glm::u8vec3(1, 1, 7));

    const auto round_trip = io::conversion::to_mat(*raster);
    REQUIRE(round_trip.has_value());
    raster->fill(glm::u8vec3(0));
    CHECK(round_trip->at<cv::Vec3b>(0, 0) == cv::Vec3b(1, 1, 7));
}

TEST_CASE("io::conversion reports invalid inputs")
{
    SECTION("empty matrix")
    {
        const auto result = io::conversion::to_raster<std::uint8_t>(cv::Mat {});
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == io::conversion::ErrorCode::EmptyInput);
    }

    SECTION("multidimensional matrix")
    {
        const std::array sizes { 2, 2, 2 };
        const cv::Mat source(static_cast<int>(sizes.size()), sizes.data(), CV_8UC1);
        const auto result = io::conversion::to_raster<std::uint8_t>(source);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == io::conversion::ErrorCode::UnsupportedDimensions);
    }

    SECTION("pixel type mismatch")
    {
        const cv::Mat source(2, 2, CV_8UC3);
        const auto result = io::conversion::to_raster<std::uint8_t>(source);
        REQUIRE_FALSE(result.has_value());
        const io::conversion::Error expected {
            io::conversion::ErrorCode::PixelTypeMismatch,
            CV_8UC1,
            CV_8UC3,
        };
        CHECK(result.error() == expected);
    }

    SECTION("unsupported pixel")
    {
        struct Pixel {
            std::uint64_t value;
        };
        const cv::Mat source(2, 2, CV_8UC1);
        const auto result = io::conversion::to_raster<Pixel>(source);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == io::conversion::ErrorCode::UnsupportedPixelType);
    }

    SECTION("empty raster")
    {
        const auto result = io::conversion::to_mat(radix::Raster<std::uint8_t> {});
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == io::conversion::ErrorCode::EmptyInput);
    }
}
