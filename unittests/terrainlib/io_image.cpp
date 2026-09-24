#include <algorithm>
#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <opencv2/imgcodecs.hpp>

#include "../temporary_directory.h"
#include "io/bytes.h"
#include "io/image.h"

namespace {
namespace image = io::image;

image::RGBA8 colours()
{
    image::RGBA8 result({ 2, 2 });
    result.pixel({ 0, 0 }) = { 255, 0, 0, 255 };
    result.pixel({ 1, 0 }) = { 0, 255, 0, 128 };
    result.pixel({ 0, 1 }) = { 0, 0, 255, 0 };
    result.pixel({ 1, 1 }) = { 17, 31, 47, 0 };
    return result;
}
} // namespace

TEST_CASE("PNG preserves RGBA including hidden colour and row order", "[io-image]")
{
    const auto source = colours();
    const auto bytes = image::encode(source, image::Format::Png);
    REQUIRE(bytes);
    const auto decoded = image::decode_rgba8(*bytes);
    REQUIRE(decoded);
    CHECK(decoded->size() == source.size());
    CHECK(std::ranges::equal(decoded->bytes(), source.bytes()));
    // Independent decoder verifies that both sides of our API cannot hide a channel/row swap.
    const auto reference = cv::imdecode(*bytes, cv::IMREAD_UNCHANGED);
    REQUIRE(reference.type() == CV_8UC4);
    CHECK(reference.at<cv::Vec4b>(0, 0) == cv::Vec4b(0, 0, 255, 255));
    CHECK(reference.at<cv::Vec4b>(1, 1) == cv::Vec4b(47, 31, 17, 0));
    const auto rgb = image::decode_rgb8(*bytes);
    REQUIRE(rgb);
    CHECK(rgb->pixel({ 1, 1 }) == glm::u8vec3(17, 31, 47));
    const auto rgb_bytes = image::encode(*rgb, image::Format::Png);
    REQUIRE(rgb_bytes);
    const auto opaque = image::decode_rgba8(*rgb_bytes);
    REQUIRE(opaque);
    CHECK(opaque->pixel({ 1, 1 }) == glm::u8vec4(17, 31, 47, 255));

    const cv::Mat grey(2, 2, CV_8UC1, cv::Scalar(37));
    std::vector<std::uint8_t> grey_bytes;
    REQUIRE(cv::imencode(".png", grey, grey_bytes));
    const auto grey_rgb = image::decode_rgb8(grey_bytes);
    const auto grey_rgba = image::decode_rgba8(grey_bytes);
    REQUIRE(grey_rgb);
    REQUIRE(grey_rgba);
    CHECK(grey_rgb->pixel({ 0, 0 }) == glm::u8vec3(37, 37, 37));
    CHECK(grey_rgba->pixel({ 0, 0 }) == glm::u8vec4(37, 37, 37, 255));
}

TEST_CASE("PNG compression and JPEG quality options reach encoders", "[io-image]")
{
    image::RGB8 source({ 128, 128 });
    for (unsigned y = 0; y < source.height(); ++y) {
        for (unsigned x = 0; x < source.width(); ++x) {
            source.pixel({ x, y }) = glm::u8vec3(x * 2, y * 2, (x * 7 + y * 11) % 256);
        }
    }
    const auto plain = image::encode(source, image::Format::Png, { .png_compression = 0 });
    const auto compressed = image::encode(source, image::Format::Png, { .png_compression = 9 });
    REQUIRE(plain);
    REQUIRE(compressed);
    CHECK(compressed->size() < plain->size());
    for (const auto& bytes : { *plain, *compressed }) {
        const auto decoded = image::decode_rgb8(bytes);
        REQUIRE(decoded);
        CHECK(std::ranges::equal(decoded->bytes(), source.bytes()));
    }
    const auto low = image::encode(source, image::Format::Jpeg, { .jpeg_quality = 10 });
    const auto high = image::encode(source, image::Format::Jpeg, { .jpeg_quality = 95 });
    REQUIRE(low);
    REQUIRE(high);
    CHECK(low->size() < high->size());
    source.fill({ 255, 0, 0 });
    const auto jpeg = image::encode(source, image::Format::Jpeg);
    REQUIRE(jpeg);
    const auto reference = cv::imdecode(*jpeg, cv::IMREAD_COLOR);
    const auto red = reference.at<cv::Vec3b>(0, 0);
    CHECK(red[0] <= 2);
    CHECK(red[1] <= 2);
    CHECK(red[2] >= 253);
    const auto rgba = image::decode_rgba8(*jpeg);
    REQUIRE(rgba);
    CHECK(rgba->pixel({ 0, 0 }).w == 255);
}

TEST_CASE("image I/O rejects invalid inputs without silent alpha loss", "[io-image]")
{
    const auto source = colours();
    CHECK(image::encode(source, image::Format::Jpeg).error().code() == Error::Code::Unsupported);
    CHECK_FALSE(image::encode(source, image::Format::Png, { .png_compression = -1 }));
    CHECK_FALSE(image::encode(source, image::Format::Png, { .png_compression = 10 }));
    CHECK_FALSE(image::encode(source, image::Format::Png, { .jpeg_quality = -1 }));
    CHECK_FALSE(image::encode(source, image::Format::Png, { .jpeg_quality = 101 }));
    CHECK_FALSE(image::encode(image::RGB8 {}, image::Format::Png));
    CHECK_FALSE(image::decode_rgb8({}));
    const std::array<std::uint8_t, 4> junk { 1, 2, 3, 4 };
    CHECK(image::decode_rgba8(junk).error().code() == Error::Code::CorruptData);
}

TEST_CASE("disk image I/O preserves RGBA and honours overwrite and directory policies", "[io-image]")
{
    test::TemporaryDirectory directory;
    const auto path = directory.path() / "nested" / "image.PNG";
    auto source = colours();
    CHECK_FALSE(image::write(source, path, { .make_dirs = false }));
    REQUIRE(image::write(source, path));
    const auto original = io::read_bytes_from_path(path);
    REQUIRE(original);
    CHECK(image::write(source, path).error().code() == Error::Code::AlreadyExists);
    CHECK(*io::read_bytes_from_path(path) == *original);
    source.pixel({ 0, 0 }) = { 8, 9, 10, 11 };
    REQUIRE(image::write(source, path, { .overwrite = true }));
    const auto decoded = image::read_rgba8(path);
    REQUIRE(decoded);
    CHECK(std::ranges::equal(decoded->bytes(), source.bytes()));
    const auto rgb = image::read_rgb8(path);
    REQUIRE(rgb);
    REQUIRE(image::write(*rgb, directory.path() / "image.jpeg"));
    CHECK_FALSE(image::write(source, directory.path() / "image.unknown"));
    CHECK(image::read_rgb8(directory.path() / "missing.png").error().code() == Error::Code::NotFound);
    const auto link = directory.path() / "link.png";
    std::filesystem::create_symlink(path, link);
    CHECK_FALSE(image::write(source, link, { .overwrite = true }));
    const auto folder = directory.path() / "folder.png";
    REQUIRE(std::filesystem::create_directory(folder));
    CHECK_FALSE(image::write(source, folder, { .overwrite = true }));
}

TEST_CASE("byte writer retains legacy overwrite and supports exclusive creation", "[io-image]")
{
    test::TemporaryDirectory directory;
    const auto path = directory.path() / "bytes";
    const std::array<std::uint8_t, 3> first { 1, 2, 3 };
    const std::array<std::uint8_t, 2> second { 4, 5 };
    REQUIRE(io::write_bytes_to_path(first, path, io::WriteMode::CreateNew));
    CHECK(io::write_bytes_to_path(second, path, io::WriteMode::CreateNew).error().code() == Error::Code::AlreadyExists);
    CHECK(*io::read_bytes_from_path(path) == std::vector<std::uint8_t>(first.begin(), first.end()));
    REQUIRE(io::write_bytes_to_path(second, path));
    CHECK(*io::read_bytes_from_path(path) == std::vector<std::uint8_t>(second.begin(), second.end()));
}
