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

#include "raster_store/io/float_predictor.h"
#include "../temporary_directory.h"
#include "raster_store/storage.h"

#include <array>
#include <cstring>
#include <random>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace {
namespace schema = raster_store::io::schema;
using io::envelope::CompressionAlgorithm;
using raster_store::io::DataEncoding;
using raster_store::io::EncodedSubtile;
using raster_store::io::TileCodec;

// Writes a tile container holding one subtile with the given schema.
template <typename Schema>
void write_single_subtile(const EncodedSubtile& payload, const std::filesystem::path& path)
{
    auto subtile = io::envelope::serialize<Schema>(payload);
    REQUIRE(subtile);
    const schema::v1::TileContainer container { { { 0, subtile->size() } }, std::move(*subtile) };
    REQUIRE(io::envelope::write<schema::TileContainerSchema>(container, path, io::byte::WriteMode::Overwrite, true, CompressionAlgorithm::None));
}
} // namespace

TEST_CASE("Float32 Predictor 3 matches libtiff including plane and row boundaries", "[raster-store][codec][amoft]")
{
    const std::array<std::uint32_t, 6> bits { 0x3f800000, 0x40000000, 0xc0400000, 0x80000000, 0x7fc01234, 0x00000001 };
    // Raw Deflate strip decompressed from libtiff 4.7.0 with Predictor=3,
    // SampleFormat=IEEEFP, BitsPerSample=32, SamplesPerPixel=1, width=3.
    const std::array<std::uint8_t, 24> reference {
        0x3f, 0x01, 0x80, 0xc0, 0x80, 0x40, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x81, 0x00, 0xc0, 0x40, 0x00, 0x12, 0xee, 0x00, 0x34, 0xcd
    };
    const auto original = std::as_bytes(std::span(bits));
    io::byte::vector encoded(original.begin(), original.end());
    raster_store::io::float_predictor::encode(encoded, 3);
    CHECK(std::ranges::equal(encoded, std::as_bytes(std::span(reference))));
    // Decode the independent reference, even if encoding fails the comparison.
    const auto reference_bytes = std::as_bytes(std::span(reference));
    encoded.assign(reference_bytes.begin(), reference_bytes.end());
    raster_store::io::float_predictor::decode(encoded, 3);
    CHECK(std::ranges::equal(encoded, original));
}

TEST_CASE("AMOFT preserves all Float32 bits and attribution with both Zstd settings", "[raster-store][codec][amoft]")
{
    const auto side = GENERATE(1u, 3u, 17u, 64u);
    const auto compression = GENERATE(CompressionAlgorithm::ZstdDefaultCompression, CompressionAlgorithm::ZstdBestCompression);
    test::TemporaryDirectory directory;
    raster_store::Tile<float> tile(side);
    std::mt19937 random(42);
    for (std::size_t i = 0; i < tile.data.buffer_length(); ++i) {
        const std::uint32_t bits = random();
        std::memcpy(tile.data.bytes().data() + i * 4, &bits, 4);
        tile.attribution.buffer()[i] = static_cast<std::uint16_t>(i % 65535);
    }
    constexpr std::array<std::uint32_t, 12> special {
        0x00000000, 0x80000000, 0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7f801234, 0x00000001, 0x80000001, 0x007fffff, 0x00800000, 0x7f7fffff
    };
    const auto count = (std::min)(tile.data.buffer_length(), special.size());
    std::memcpy(tile.data.bytes().data(), special.data(), count * 4);
    tile.attribution.fill(0); // Values under NoData must also retain every bit.
    tile.attribution.buffer()[side - 1] = 65534;
    const io::byte::vector original(tile.data.bytes().begin(), tile.data.bytes().end());
    const TileCodec<float> writer(glm::uvec2(side), DataEncoding::FloatPredictor, compression);
    const auto path = directory.path() / "tile";
    const auto hash = writer.write(path, tile);
    REQUIRE(hash);
    CHECK(writer.paths(path).front().extension() == ".amoft");
    CHECK(std::ranges::equal(tile.data.bytes(), original));
    const auto container = io::envelope::read<schema::TileContainerSchema>(writer.paths(path).front());
    REQUIRE(container);
    const auto subtile = io::envelope::deserialize_prefix(container->data);
    REQUIRE(subtile);
    CHECK(subtile->first.class_name == "raster_store.FloatTile");
    CHECK(subtile->first.compression_algorithm == compression);
    // The reader uses the compression setting stored in the envelope.
    const auto decoded = TileCodec<float>(glm::uvec2(side), DataEncoding::FloatPredictor).read(path, *hash);
    REQUIRE(decoded);
    CHECK(std::ranges::equal(decoded->data.bytes(), original));
    CHECK(std::ranges::equal(decoded->attribution.bytes(), tile.attribution.bytes()));
    CHECK(writer.write(path, tile).error().code() == Error::Code::AlreadyExists);
    auto wrong_hash = *hash;
    wrong_hash[0] ^= std::byte { 1 };
    CHECK_FALSE(writer.read(path, wrong_hash));
}

TEST_CASE("AMOFT rejects wrong dimensions, byte counts, and envelope classes", "[raster-store][codec][amoft]")
{
    test::TemporaryDirectory directory;
    const auto path = directory.path() / "tile";
    const TileCodec<float> codec(glm::uvec2(3), DataEncoding::FloatPredictor);
    EncodedSubtile payload { 3, 3, io::byte::vector(36), io::byte::vector(18) };
    SECTION("width mismatch") { payload.width = 4; }
    SECTION("empty data") { payload.data.clear(); }
    SECTION("short data") { payload.data.pop_back(); }
    SECTION("long data") { payload.data.push_back(std::byte { 0 }); }
    SECTION("short attribution") { payload.source_attribution.pop_back(); }
    SECTION("zero dimensions") { payload.width = payload.height = 0; }
    write_single_subtile<schema::FloatPredictorSubtileSchema>(payload, path.string() + ".amoft");
    CHECK(codec.read(path).error().code() == Error::Code::CorruptData);
    write_single_subtile<schema::RawRasterSubtileSchema>(payload, path.string() + ".amoft");
    CHECK(codec.read(path).error().code() == Error::Code::CorruptData);
    CHECK(codec.write(path, raster_store::Tile<float>(4)).error().code() == Error::Code::InvalidInput);
}

TEST_CASE("AMOFT snapshots require Float32 and reopen with exact data", "[raster-store][codec][amoft]")
{
    CHECK(TileCodec<double>::from_name("amoft", glm::uvec2(4)).error().code() == Error::Code::Unsupported);
    CHECK(TileCodec<std::uint32_t>::from_name("amoft", glm::uvec2(4)).error().code() == Error::Code::Unsupported);
    CHECK(TileCodec<glm::vec3>::from_name("amoft", glm::uvec2(4)).error().code() == Error::Code::Unsupported);
    CHECK(TileCodec<glm::u8vec3>::from_name("amoft", glm::uvec2(4)).error().code() == Error::Code::Unsupported);
    test::TemporaryDirectory directory;
    const std::string table = R"([{"spatial_resolution":1,"acquisition_date":"","ingestion_date":"","copyright":"","copyright_link":"","license":""}])";
    REQUIRE(io::byte::write(std::as_bytes(std::span(table)), directory.path() / raster_store::attribution::file_name, io::byte::WriteMode::CreateNew));
    raster_store::storage::CreateOptions options;
    options.nominal_tile_size = 4;
    options.codec_selector = "amoft";
    auto created = raster_store::storage::create<float>(directory.path() / "snapshot", options);
    REQUIRE(created);
    raster_store::Tile<float> tile(4);
    tile.data.fill(-1.25f);
    const radix::tile::Id key { 0, { 0, 0 } };
    REQUIRE(created->first->save(key, tile));
    REQUIRE(raster_store::storage::publish(std::move(created->first)));
    auto opened = raster_store::storage::open<float>(directory.path() / "snapshot");
    REQUIRE(opened);
    CHECK(opened->second->codec_selector == "amoft");
    const auto decoded = opened->first->load(key);
    REQUIRE(decoded);
    CHECK(std::ranges::equal(decoded->data.bytes(), tile.data.bytes()));
}
