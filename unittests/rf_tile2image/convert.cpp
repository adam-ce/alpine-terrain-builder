#include "convert.h"
#include "../temporary_directory.h"
#include "io/bytes.h"
#include "raster_store/io/TileCodec.h"
#include "raster_store/io/manifest.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>
#include <type_traits>

#include "io/image.h"
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace {

namespace manifest = raster_store::io::manifest;

template <typename PixelType>
rf_tile2image::Options fixture(const std::filesystem::path& root,
    const raster_store::Tile<PixelType>& tile,
    const raster_store::pixel::Mapping mapping = raster_store::pixel::default_mapping<PixelType>,
    const unsigned halo = 0)
{
    manifest::Metadata metadata {
        "zoom_x_y_google", raster_store::pixel::identifier<PixelType>(), "amort", tile.data.width(), tile.data.width() - 2 * halo, halo, mapping
    };
    REQUIRE(manifest::write_metadata(metadata, root));
    const auto node = root / "tiles" / "13" / "4352" / "2868";
    REQUIRE(raster_store::io::TileCodec<PixelType>(tile.data.size()).write(node, tile));
    rf_tile2image::Options options;
    options.input = node.string() + ".amort";
    return options;
}

void check_colour(const glm::u8vec3 actual, const glm::u8vec3 expected)
{
    CHECK(actual[0] == expected[0]);
    CHECK(actual[1] == expected[1]);
    CHECK(actual[2] == expected[2]);
}

} // namespace

TEMPLATE_TEST_CASE("RF previews dispatch every scalar type",
    "[rf-tile2image]",
    std::int8_t,
    std::uint8_t,
    std::int16_t,
    std::uint16_t,
    std::int32_t,
    std::uint32_t,
    std::int64_t,
    std::uint64_t,
    float,
    double)
{
    test::TemporaryDirectory directory;
    raster_store::Tile<TestType> tile(2);
    const auto highest = (std::numeric_limits<TestType>::max)();
    tile.data.buffer()[0] = highest - TestType { 1 };
    tile.data.buffer()[1] = highest;
    tile.data.buffer()[2] = TestType { 0 };
    tile.data.buffer()[3] = TestType { 1 };
    rf_tile2image::Options options = fixture(directory.path(), tile);
    auto images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(images->data.pixel({ unsigned(1), unsigned(0) }), { 255, 255, 255 });
    check_colour(images->data.pixel({ unsigned(0), unsigned(1) }), { 0, 0, 0 });
    if constexpr (std::is_integral_v<TestType>) {
        tile.data.fill(highest);
        tile.data.buffer()[0] = highest - TestType { 1 };
        options = fixture(directory.path(), tile);
        images = rf_tile2image::render_tile(options);
        REQUIRE(images);
        check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 0, 0, 0 });
        check_colour(images->data.pixel({ unsigned(1), unsigned(0) }), { 255, 255, 255 });
    }
}

TEST_CASE("RF constant diagnostics yield to either range override", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    const auto value = GENERATE(0.0f, 5.0f, -5.0f);
    raster_store::Tile<float> tile(2);
    tile.data.fill(value);
    auto options = fixture(directory.path(), tile);
    auto images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(
        images->data.pixel({ unsigned(0), unsigned(0) }), value == 0 ? glm::u8vec3(0, 0, 0) : (value > 0 ? glm::u8vec3(255, 0, 0) : glm::u8vec3(0, 0, 255)));
    options.minimum = value - 1;
    images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 255, 255, 255 });
    options.minimum.reset();
    options.maximum = value + 1;
    images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 0, 0, 0 });
    options.minimum = value - 1;
    images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    // Standard Cubehelix midpoint, rounded to bytes (RGB).
    check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 160, 121, 73 });
}

TEST_CASE("RF nonfinite pixels are magenta and unattributed values still define the range", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<double> tile(4);
    tile.data.fill(std::numeric_limits<double>::quiet_NaN());
    tile.data.buffer()[0] = -std::numeric_limits<double>::infinity();
    tile.data.buffer()[1] = std::numeric_limits<double>::infinity();
    auto options = fixture(directory.path(), tile);
    auto images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            check_colour(images->data.pixel({ unsigned(column), unsigned(row) }), { 255, 0, 255 });
        }
    }
    tile.data.buffer()[2] = -10;
    tile.data.buffer()[3] = 10;
    tile.source_attribution.buffer()[3] = 1;
    options = fixture(directory.path(), tile);
    images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 255, 0, 255 });
    check_colour(images->data.pixel({ unsigned(2), unsigned(0) }), { 0, 0, 0 });
    check_colour(images->data.pixel({ unsigned(3), unsigned(0) }), { 255, 255, 255 });
    options.minimum = -5;
    options.maximum = 5;
    images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(images->data.pixel({ unsigned(2), unsigned(0) }), { 0, 0, 0 });
    check_colour(images->data.pixel({ unsigned(3), unsigned(0) }), { 255, 255, 255 });
}

TEST_CASE("RF scalar ranges reject invalid limits and handle extreme values", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<double> tile(2);
    tile.data.buffer()[0] = -(std::numeric_limits<double>::max)();
    tile.data.buffer()[1] = (std::numeric_limits<double>::max)();
    auto options = fixture(directory.path(), tile);
    auto images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 0, 0, 0 });
    check_colour(images->data.pixel({ unsigned(1), unsigned(0) }), { 255, 255, 255 });
    check_colour(images->data.pixel({ unsigned(0), unsigned(1) }), { 160, 121, 73 });
    options.minimum = std::numeric_limits<long double>::quiet_NaN();
    CHECK_FALSE(rf_tile2image::render_tile(options));
    options.minimum = 1;
    options.maximum = 1;
    CHECK_FALSE(rf_tile2image::render_tile(options));
    options.maximum = -1;
    CHECK_FALSE(rf_tile2image::render_tile(options));
    options.minimum.reset();
    options.maximum = -(std::numeric_limits<double>::max)();
    CHECK_FALSE(rf_tile2image::render_tile(options));
}

TEST_CASE("RF attribution hashes are unique with contrasting neighbours", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<std::uint8_t> tile(256);
    for (std::uint32_t index = 0; index < 65535; ++index) {
        tile.source_attribution.buffer()[index] = static_cast<std::uint16_t>(index);
    }
    auto options = fixture(directory.path(), tile);
    auto images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    const auto* colours = images->attribution.data();
    std::set<std::uint32_t> seen;
    for (std::uint32_t index = 0; index < 65535; ++index) {
        const auto colour = colours[index];
        const auto packed = std::uint32_t(colour[0]) | (std::uint32_t(colour[1]) << 8) | (std::uint32_t(colour[2]) << 16);
        REQUIRE(seen.insert(packed).second);
        if (index != 0) {
            REQUIRE(packed != 0);
            const glm::dvec3 delta = glm::dvec3(colour) - glm::dvec3(colours[index - 1]);
            REQUIRE(glm::dot(delta, delta) > 160 * 160);
        }
    }
    check_colour(colours[0], { 0, 0, 0 });
    check_colour(colours[1], { 0x9E, 0x37, 0x79 });
    tile.source_attribution.buffer()[0] = 65535;
    options = fixture(directory.path(), tile);
    CHECK_FALSE(rf_tile2image::render_tile(options));
}

TEMPLATE_TEST_CASE("RF imagery preserves RGB channels, halo, orientation, and ignores alpha", "[rf-tile2image]", glm::u8vec3, glm::u8vec4)
{
    test::TemporaryDirectory directory;
    raster_store::Tile<TestType> tile(6);
    tile.data.fill(TestType(0));
    tile.data.pixel({ 0, 0 }).x = 255;
    tile.data.pixel({ 5, 5 }).z = 255;
    tile.source_attribution.pixel({ 0, 0 }) = 1;
    const rf_tile2image::Options options = fixture(directory.path(), tile, raster_store::pixel::Mapping::SRGBA, 1);
    auto images = rf_tile2image::render_tile(options);
    REQUIRE(images);
    CHECK(images->data.height() == 6);
    CHECK(images->data.width() == 6);
    check_colour(images->data.pixel({ unsigned(0), unsigned(0) }), { 255, 0, 0 });
    check_colour(images->data.pixel({ unsigned(5), unsigned(5) }), { 0, 0, 255 });
    check_colour(images->attribution.pixel({ unsigned(0), unsigned(0) }), { 0x9E, 0x37, 0x79 });
}

TEST_CASE("RF previews reject unsupported mappings, vectors, and imagery ranges", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<glm::u8vec3> rgb(2);
    auto options = fixture(directory.path(), rgb);
    options.minimum = 0;
    CHECK_FALSE(rf_tile2image::render_tile(options));
    options = fixture(directory.path(), rgb, raster_store::pixel::Mapping::Linear);
    CHECK_FALSE(rf_tile2image::render_tile(options));
    raster_store::Tile<float> scalar(2);
    options = fixture(directory.path(), scalar, raster_store::pixel::Mapping::SRGBA);
    CHECK_FALSE(rf_tile2image::render_tile(options));
    raster_store::Tile<glm::vec2> vector(2);
    options = fixture(directory.path(), vector);
    CHECK_FALSE(rf_tile2image::render_tile(options));
}

TEST_CASE("RF metadata discovery, explicit metadata, corruption and size checks", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<float> tile(2);
    auto options = fixture(directory.path(), tile);
    const auto original = options.input;
    const auto isolated = directory.path() / "copy.amort";
    std::filesystem::copy_file(original, isolated);
    const auto saved_metadata = directory.path() / "metadata.saved";
    std::filesystem::rename(directory.path() / manifest::metadata_file_name, saved_metadata);
    options.input = isolated;
    CHECK_FALSE(rf_tile2image::render_tile(options));
    options.metadata = saved_metadata;
    REQUIRE(rf_tile2image::render_tile(options));
    options.input = directory.path() / "missing.amort";
    CHECK_FALSE(rf_tile2image::render_tile(options));
    options.input = isolated;
    auto metadata = io::envelope::read_from_path<manifest::MetadataSchema>(saved_metadata);
    REQUIRE(metadata);
    metadata->stored_tile_size = metadata->nominal_tile_size = 4;
    REQUIRE(io::envelope::write_to_path<manifest::MetadataSchema>(*metadata, saved_metadata));
    CHECK_FALSE(rf_tile2image::render_tile(options));
    const std::array<std::uint8_t, 3> junk { 1, 2, 3 };
    REQUIRE(io::write_bytes_to_path(junk, saved_metadata));
    CHECK_FALSE(rf_tile2image::render_tile(options));
}

TEST_CASE("RF conversion writes JPEG and lossless PNG and protects both existing outputs", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<float> tile(16);
    tile.data.fill(10);
    tile.source_attribution.fill(1);
    auto options = fixture(directory.path(), tile);
    options.output_directory = directory.path() / "images";
    auto paths = rf_tile2image::convert(options);
    REQUIRE(paths);
    const auto jpeg = io::image::read_rgb8(paths->data);
    REQUIRE(jpeg);
    const auto png = io::image::read_rgb8(paths->attribution);
    REQUIRE(png);
    REQUIRE(jpeg->size() == glm::uvec2(16, 16));
    REQUIRE(png->size() == jpeg->size());
    const auto red = jpeg->pixel({ 0, 0 });
    CHECK(red[0] >= 253);
    CHECK(red[1] <= 2);
    CHECK(red[2] <= 2);
    check_colour(png->pixel({ 0, 0 }), { 0x9E, 0x37, 0x79 });
    const auto before = io::read_bytes_from_path(paths->attribution).value();
    CHECK_FALSE(rf_tile2image::convert(options));
    std::filesystem::remove(paths->data);
    CHECK_FALSE(rf_tile2image::convert(options));
    CHECK_FALSE(std::filesystem::exists(paths->data));
    CHECK(io::read_bytes_from_path(paths->attribution).value() == before);
    options.overwrite = true;
    REQUIRE(rf_tile2image::convert(options));
    const auto png_bytes = io::read_bytes_from_path(paths->attribution).value();
    CHECK(png_bytes == before);
    const auto jpeg_bytes = io::read_bytes_from_path(paths->data).value();
    REQUIRE(jpeg_bytes.size() >= 2);
    CHECK(jpeg_bytes[0] == 0xFF);
    CHECK(jpeg_bytes[1] == 0xD8);
    REQUIRE(png_bytes.size() >= 8);
    CHECK(png_bytes[0] == 0x89);
    CHECK(png_bytes[1] == 'P');
    CHECK(png_bytes[2] == 'N');
    CHECK(png_bytes[3] == 'G');
}

TEST_CASE("RF CLI accepts independent range options and reports errors", "[rf-tile2image]")
{
    test::TemporaryDirectory directory;
    raster_store::Tile<float> tile(16);
    tile.data.fill(5);
    const auto options = fixture(directory.path(), tile);
    const auto output = directory.path() / "cli-output";
    // These paths are created by the fixture and contain no shell metacharacters.
    const std::string command
        = std::string("\"") + ALP_RF_TILE2IMAGE_PATH + "\" \"" + options.input.string() + "\" --output-dir \"" + output.string() + "\" --min 0 --max 10";
    REQUIRE(std::system(command.c_str()) == 0);
    CHECK(std::filesystem::exists(output / "2868.jpg"));
    CHECK(std::filesystem::exists(output / "2868.png"));
    CHECK(std::system(command.c_str()) != 0);
    CHECK(std::system((command + " --overwrite").c_str()) == 0);
    CHECK(std::system((command + " --overwrite --unknown").c_str()) != 0);
}
