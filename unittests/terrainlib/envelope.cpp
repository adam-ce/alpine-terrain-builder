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

#include "../catch2_helpers.h"
#include "../temporary_directory.h"

#include "io/envelope.h"

#include <zpp_bits.h>
#include <zstd.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

namespace v1 {

    struct Payload {
        std::uint32_t id;
        std::string name;

        bool operator==(const Payload&) const = default;
    };

} // namespace v1

namespace v2 {

    struct Payload {
        std::uint64_t id;
        std::string name;
        bool enabled;

        static Payload from_previous(v1::Payload previous)
        {
            return {
                .id = previous.id,
                .name = std::move(previous.name),
                .enabled = true,
            };
        }

        bool operator==(const Payload&) const = default;
    };

} // namespace v2

namespace v3 {

    struct Payload {
        std::uint64_t id;
        std::string label;
        bool enabled;
        std::vector<std::int32_t> samples;

        static Payload from_previous(v2::Payload previous)
        {
            return {
                .id = previous.id,
                .label = std::move(previous.name),
                .enabled = previous.enabled,
                .samples = {},
            };
        }

        bool operator==(const Payload&) const = default;
    };

} // namespace v3

using Schema = io::envelope::
    PayloadSchema<"test.Payload", io::envelope::Version<1, v1::Payload>, io::envelope::Version<2, v2::Payload>, io::envelope::Version<3, v3::Payload>>;

constexpr std::array zstd_compression_algorithms {
    io::envelope::CompressionAlgorithm::ZstdBestCompression,
    io::envelope::CompressionAlgorithm::ZstdDefaultCompression,
};

constexpr std::array compression_algorithms {
    io::envelope::CompressionAlgorithm::None,
    io::envelope::CompressionAlgorithm::ZstdBestCompression,
    io::envelope::CompressionAlgorithm::ZstdDefaultCompression,
};

constexpr std::array hash_algorithms {
    io::hash::Algorithm::None,
    io::hash::Algorithm::Xxh3_64,
};

static_assert(std::is_aggregate_v<v1::Payload>);
static_assert(std::is_aggregate_v<v2::Payload>);
static_assert(std::is_aggregate_v<v3::Payload>);
static_assert(Schema::class_name == "test.Payload");
static_assert(Schema::latest_version == 3);
static_assert(std::same_as<Schema::latest_type, v3::Payload>);
static_assert(std::same_as<Schema::payload_type<1>, v1::Payload>);

template <typename Value>
std::vector<std::byte> encode_value(const Value& value)
{
    std::vector<std::byte> bytes;
    zpp::bits::out output(bytes);
    output(value).or_throw();
    return bytes;
}

std::vector<std::byte> encode_envelope(const io::envelope::Envelope& envelope) { return encode_value(envelope); }

io::envelope::Envelope decode_envelope(const std::vector<std::byte>& bytes)
{
    io::envelope::Envelope envelope {};
    zpp::bits::in input(bytes);
    input(envelope).or_throw();
    return envelope;
}

// Writes a frame with a checksum, which the reader accepts but does not require.
std::vector<std::byte> compress_without_content_size(const std::vector<std::byte>& uncompressed_data)
{
    const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context { ZSTD_createCCtx(), &ZSTD_freeCCtx };
    if (!context || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, 0))
        || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1))) {
        throw std::runtime_error { "could not configure zstd test context" };
    }

    std::vector<std::byte> compressed_data(ZSTD_compressBound(uncompressed_data.size()));
    const std::size_t compressed_size
        = ZSTD_compress2(context.get(), compressed_data.data(), compressed_data.size(), uncompressed_data.data(), uncompressed_data.size());
    if (ZSTD_isError(compressed_size)) {
        throw std::runtime_error { "could not create zstd test data" };
    }
    compressed_data.resize(compressed_size);
    return compressed_data;
}

template <typename Result>
void check_error(const Result& result, const Error::Code expected)
{
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == expected);
}

void write_file(const std::filesystem::path& path, const std::span<const std::byte> bytes) { REQUIRE(io::write_bytes_to_path(bytes, path)); }

} // namespace

TEST_CASE("Envelope round trips the latest payload version")
{
    const v3::Payload expected {
        .id = 42,
        .label = "latest",
        .enabled = false,
        .samples = { 1, 2, 3 },
    };

    const auto bytes = io::envelope::serialize<Schema, 3>(expected);
    REQUIRE(bytes.has_value());

    const auto envelope = decode_envelope(*bytes);
    CHECK(envelope.header.magic == io::envelope::magic);
    CHECK(envelope.header.class_name == Schema::class_name);
    CHECK(envelope.header.class_version == 3);
    CHECK(envelope.header.hash_algorithm == io::hash::Algorithm::Xxh3_64);
    CHECK(envelope.header.hash == io::hash::data(encode_value(expected), io::hash::Algorithm::Xxh3_64));
    CHECK(envelope.header.compression_algorithm == io::envelope::CompressionAlgorithm::ZstdDefaultCompression);
    CHECK(envelope.header.uncompressed_size == encode_value(expected).size());

    const auto result = io::envelope::deserialize<Schema>(*bytes);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("Envelope round trips payloads containing GLM types")
{
    struct Payload {
        glm::dvec3 position;
        glm::mat4 transform;
        std::vector<glm::vec2> texture_coordinates;
        bool operator==(const Payload&) const = default;
    };
    using GlmSchema = io::envelope::PayloadSchema<"test.GlmPayload", io::envelope::Version<1, Payload>>;
    const Payload expected {
        .position = { 1.0, -2.0, 3.5 },
        .transform = glm::mat4(2.0f),
        .texture_coordinates = { { 0.0f, 0.5f }, { 1.0f, 0.75f } },
    };
    const auto bytes = io::envelope::serialize<GlmSchema>(expected);
    REQUIRE(bytes.has_value());
    const auto result = io::envelope::deserialize<GlmSchema>(*bytes);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("Envelope upgrades older payload versions")
{
    SECTION("version 1 is upgraded through every subsequent version")
    {
        const v1::Payload original { .id = 7, .name = "version one" };
        const auto bytes = io::envelope::serialize<Schema, 1>(original);
        REQUIRE(bytes.has_value());

        const auto result = io::envelope::deserialize<Schema>(*bytes);
        REQUIRE(result.has_value());
        CHECK(*result
            == v3::Payload {
                .id = 7,
                .label = "version one",
                .enabled = true,
                .samples = {},
            });
    }

    SECTION("version 2 is upgraded to the latest version")
    {
        const v2::Payload original { .id = 9, .name = "version two", .enabled = false };
        const auto bytes = io::envelope::serialize<Schema, 2>(original);
        REQUIRE(bytes.has_value());

        const auto result = io::envelope::deserialize<Schema>(*bytes);
        REQUIRE(result.has_value());
        CHECK(*result
            == v3::Payload {
                .id = 9,
                .label = "version two",
                .enabled = false,
                .samples = {},
            });
    }
}

TEST_CASE("Envelope supports uncompressed data without a hash")
{
    const v3::Payload expected {
        .id = 11,
        .label = "plain",
        .enabled = true,
        .samples = { 5, 8 },
    };
    const auto bytes = io::envelope::serialize<Schema, 3>(expected, io::envelope::CompressionAlgorithm::None, io::hash::Algorithm::None);
    REQUIRE(bytes.has_value());

    const auto envelope = decode_envelope(*bytes);
    CHECK(envelope.header.compression_algorithm == io::envelope::CompressionAlgorithm::None);
    CHECK(envelope.header.hash_algorithm == io::hash::Algorithm::None);
    CHECK(envelope.header.hash.empty());
    CHECK(envelope.header.uncompressed_size == envelope.compressed_data.size());

    const auto result = io::envelope::deserialize<Schema>(*bytes);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("Envelope round trips every compression and hash combination")
{
    const v3::Payload expected {
        .id = 12,
        .label = "combination",
        .enabled = true,
        .samples = { 3, 5, 8 },
    };
    for (const auto compression_algorithm : compression_algorithms) {
        for (const auto hash_algorithm : hash_algorithms) {
            CAPTURE(compression_algorithm, hash_algorithm);
            const auto bytes = io::envelope::serialize<Schema, 3>(expected, compression_algorithm, hash_algorithm);
            REQUIRE(bytes.has_value());

            const auto envelope = decode_envelope(*bytes);
            CHECK(envelope.header.compression_algorithm == compression_algorithm);
            CHECK(envelope.header.hash_algorithm == hash_algorithm);
            CHECK(envelope.header.hash == io::hash::data(encode_value(expected), hash_algorithm));

            const auto result = io::envelope::deserialize<Schema>(*bytes);
            REQUIRE(result.has_value());
            CHECK(*result == expected);
        }
    }
}

TEST_CASE("XXH3-64 detects corrupted envelope payloads")
{
    const v3::Payload payload { .id = 15, .label = "protected by XXH3-64", .enabled = true, .samples = { 13, 21 } };

    SECTION("without compression")
    {
        const auto bytes = io::envelope::serialize<Schema, 3>(payload, io::envelope::CompressionAlgorithm::None);
        REQUIRE(bytes.has_value());
        auto envelope = decode_envelope(*bytes);
        envelope.compressed_data.back() ^= std::byte { 0x01 };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("with zstd compression")
    {
        for (const auto compression_algorithm : zstd_compression_algorithms) {
            CAPTURE(compression_algorithm);
            const auto bytes = io::envelope::serialize<Schema, 3>(payload, compression_algorithm);
            REQUIRE(bytes.has_value());
            auto envelope = decode_envelope(*bytes);
            envelope.compressed_data.back() ^= std::byte { 0x01 };
            check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
        }
    }

    SECTION("older version")
    {
        const v1::Payload original { .id = 16, .name = "xxh3 version one" };
        const auto bytes = io::envelope::serialize<Schema, 1>(original, io::envelope::CompressionAlgorithm::None);
        REQUIRE(bytes.has_value());
        auto envelope = decode_envelope(*bytes);
        REQUIRE(io::envelope::deserialize<Schema>(encode_envelope(envelope)).has_value());
        envelope.compressed_data.front() ^= std::byte { 0x01 };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }
}

TEST_CASE("Compression round trips and enforces its size limit")
{
    const std::vector<std::byte> original(4096, std::byte { 0x2a });
    for (const auto compression_algorithm : zstd_compression_algorithms) {
        CAPTURE(compression_algorithm);
        const auto compressed = io::envelope::compress(original, compression_algorithm);
        REQUIRE(compressed.has_value());
        CHECK(compressed->size() < original.size());

        const auto result = io::envelope::decompress(*compressed, compression_algorithm);
        REQUIRE(result.has_value());
        CHECK(*result == original);

        const auto empty_compressed = io::envelope::compress({}, compression_algorithm);
        REQUIRE(empty_compressed.has_value());
        const auto empty_result = io::envelope::decompress(*empty_compressed, compression_algorithm);
        REQUIRE(empty_result.has_value());
        CHECK(empty_result->empty());

        check_error(io::envelope::decompress(*compressed, compression_algorithm, original.size() - 1), Error::Code::ResourceExhausted);
    }

    const auto uncompressed = io::envelope::compress(original, io::envelope::CompressionAlgorithm::None);
    REQUIRE(uncompressed.has_value());
    CHECK(*uncompressed == original);
    check_error(io::envelope::decompress(*uncompressed, io::envelope::CompressionAlgorithm::None, original.size() - 1), Error::Code::ResourceExhausted);
}

TEST_CASE("Decompression uses its maximum when the format omits the content size")
{
    const std::vector<std::byte> original(4096, std::byte { 0x37 });
    const auto compressed_data = compress_without_content_size(original);

    for (const auto compression_algorithm : zstd_compression_algorithms) {
        CAPTURE(compression_algorithm);
        const auto result = io::envelope::decompress(compressed_data, compression_algorithm, original.size());
        REQUIRE(result.has_value());
        CHECK(*result == original);

        check_error(io::envelope::decompress(compressed_data, compression_algorithm, original.size() - 1), Error::Code::ResourceExhausted);
    }
}

TEST_CASE("Envelope rejects incompatible metadata")
{
    const v3::Payload payload { .id = 1, .label = "metadata", .enabled = true, .samples = {} };
    const auto serialized = io::envelope::serialize<Schema, 3>(payload);
    REQUIRE(serialized.has_value());

    SECTION("magic")
    {
        auto envelope = decode_envelope(*serialized);
        ++envelope.header.magic;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("magic of the previous format")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.magic = 0xF5FBD3EF919428CAULL;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("class name")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.class_name = "other.Payload";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("class name exceeds its limit")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.class_name = std::string(io::envelope::max_class_name_size + 1, 'x');
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("class version")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.class_version = 99;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::Unsupported);
    }

    SECTION("hash algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.hash_algorithm = static_cast<io::hash::Algorithm>(99);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::Unsupported);
    }

    SECTION("compression algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.compression_algorithm = static_cast<io::envelope::CompressionAlgorithm>(99);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::Unsupported);
    }

    SECTION("hash for the None algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.hash_algorithm = io::hash::Algorithm::None;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("incorrect hash")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.hash.front() ^= std::byte { 0x01 };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("hash with the wrong length")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.hash.pop_back();
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("hash exceeds its limit")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.hash.resize(io::envelope::max_hash_size + 1);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("uncompressed size is smaller than the payload")
    {
        auto envelope = decode_envelope(*serialized);
        --envelope.header.uncompressed_size;
        const auto result = io::envelope::deserialize<Schema>(encode_envelope(envelope));
        check_error(result, Error::Code::CorruptData);
        CHECK(result.error().to_string().find("reclassified ResourceExhausted -> CorruptData") != std::string::npos);
    }

    SECTION("uncompressed size is larger than the payload")
    {
        auto envelope = decode_envelope(*serialized);
        ++envelope.header.uncompressed_size;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("zero uncompressed size does not mean unspecified")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.uncompressed_size = 0;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("uncompressed size exceeds the hard limit")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.header.uncompressed_size = io::envelope::default_max_decompressed_size + 1;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::ResourceExhausted);
    }

    SECTION("uncompressed size exceeds a caller limit")
    {
        auto envelope = decode_envelope(*serialized);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope), static_cast<std::size_t>(envelope.header.uncompressed_size - 1)),
            Error::Code::ResourceExhausted);
    }
}

TEST_CASE("Envelope validates the size of uncompressed data")
{
    const v3::Payload payload { .id = 2, .label = "plain", .enabled = true, .samples = { 1 } };
    const auto serialized = io::envelope::serialize<Schema, 3>(payload, io::envelope::CompressionAlgorithm::None, io::hash::Algorithm::None);
    REQUIRE(serialized.has_value());

    SECTION("declared size is smaller")
    {
        auto envelope = decode_envelope(*serialized);
        --envelope.header.uncompressed_size;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }

    SECTION("declared size is larger")
    {
        auto envelope = decode_envelope(*serialized);
        ++envelope.header.uncompressed_size;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }
}

TEST_CASE("Envelope reports malformed serialized data")
{
    SECTION("envelope")
    {
        const std::vector<std::byte> malformed { std::byte { 0x01 }, std::byte { 0x02 } };
        check_error(io::envelope::deserialize<Schema>(malformed), Error::Code::CorruptData);
    }

    SECTION("trailing bytes")
    {
        const v3::Payload payload { .id = 3, .label = "trailing", .enabled = true, .samples = {} };
        auto serialized = io::envelope::serialize<Schema, 3>(payload);
        REQUIRE(serialized.has_value());
        serialized->push_back(std::byte { 0 });
        check_error(io::envelope::deserialize<Schema>(*serialized), Error::Code::CorruptData);
    }

    SECTION("payload")
    {
        const io::envelope::Envelope envelope {
            .header = {
                .magic = io::envelope::magic,
                .class_name = std::string { Schema::class_name },
                .class_version = 3,
                .hash_algorithm = io::hash::Algorithm::None,
                .hash = {},
                .compression_algorithm = io::envelope::CompressionAlgorithm::None,
                .uncompressed_size = 1,
            },
            .compressed_data = { std::byte { 0x01 } },
        };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), Error::Code::CorruptData);
    }
}

TEST_CASE("Envelope headers are read without the payload")
{
    test::TemporaryDirectory directory("envelope-header");
    const auto path = directory.path() / "payload.envelope";

    SECTION("the header matches a full read")
    {
        const v3::Payload payload { .id = 21, .label = std::string(4096, 'h'), .enabled = true, .samples = { 1, 2, 3 } };
        REQUIRE(io::envelope::write_to_path<Schema>(payload, path));
        const auto header = io::envelope::read_header(path);
        REQUIRE(header.has_value());
        CHECK(header->magic == io::envelope::magic);
        CHECK(header->class_name == Schema::class_name);
        CHECK(header->class_version == Schema::latest_version);
        CHECK(header->hash_algorithm == io::hash::Algorithm::Xxh3_64);
        CHECK(header->compression_algorithm == io::envelope::CompressionAlgorithm::ZstdDefaultCompression);

        const auto full = io::envelope::read_from_path<Schema>(path);
        REQUIRE(full.has_value());
        CHECK(header->uncompressed_size == encode_value(*full).size());
        CHECK(header->hash == io::hash::data(encode_value(*full), io::hash::Algorithm::Xxh3_64));
    }

    SECTION("the class name is not checked")
    {
        io::envelope::Envelope envelope = decode_envelope(io::envelope::serialize<Schema, 3>({}).value());
        envelope.header.class_name = "other.Payload";
        write_file(path, encode_envelope(envelope));
        const auto header = io::envelope::read_header(path);
        REQUIRE(header.has_value());
        CHECK(header->class_name == "other.Payload");
    }

    SECTION("a header of the maximum size")
    {
        io::envelope::Envelope envelope = decode_envelope(io::envelope::serialize<Schema, 3>({}).value());
        envelope.header.class_name = std::string(io::envelope::max_class_name_size, 'n');
        envelope.header.hash = std::vector<std::byte>(io::envelope::max_hash_size, std::byte { 0x5a });
        envelope.compressed_data = std::vector<std::byte>(1024, std::byte { 0x11 });
        const auto bytes = encode_envelope(envelope);
        CHECK(encode_value(envelope.header).size() == io::envelope::max_header_size);
        write_file(path, bytes);
        const auto header = io::envelope::read_header(path);
        REQUIRE(header.has_value());
        CHECK(header->class_name == envelope.header.class_name);
        CHECK(header->hash == envelope.header.hash);
    }

    SECTION("a file shorter than the maximum header")
    {
        const io::envelope::Envelope envelope {
            .header = {
                .magic = io::envelope::magic,
                .class_name = "a",
                .class_version = 1,
                .hash_algorithm = io::hash::Algorithm::None,
                .hash = {},
                .compression_algorithm = io::envelope::CompressionAlgorithm::None,
                .uncompressed_size = 0,
            },
            .compressed_data = {},
        };
        const auto bytes = encode_envelope(envelope);
        REQUIRE(bytes.size() < io::envelope::max_header_size);
        write_file(path, bytes);
        const auto header = io::envelope::read_header(path);
        REQUIRE(header.has_value());
        CHECK(header->class_name == "a");
        CHECK(header->hash.empty());
    }

    SECTION("a truncated header")
    {
        const auto bytes = io::envelope::serialize<Schema, 3>({}).value();
        write_file(path, std::span(bytes).first(20));
        check_error(io::envelope::read_header(path), Error::Code::CorruptData);
    }

    SECTION("a corrupt class name length is rejected")
    {
        auto bytes = io::envelope::serialize<Schema, 3>({}).value();
        // The class name length follows the 8-byte magic.
        std::fill_n(bytes.begin() + 8, 4, std::byte { 0xff });
        write_file(path, bytes);
        check_error(io::envelope::read_header(path), Error::Code::CorruptData);
        check_error(io::envelope::deserialize<Schema>(bytes), Error::Code::CorruptData);
    }

    SECTION("an invalid magic")
    {
        io::envelope::Envelope envelope = decode_envelope(io::envelope::serialize<Schema, 3>({}).value());
        envelope.header.magic = 0xF5FBD3EF919428CAULL;
        write_file(path, encode_envelope(envelope));
        check_error(io::envelope::read_header(path), Error::Code::CorruptData);
    }

    SECTION("unknown enumerators")
    {
        io::envelope::Envelope envelope = decode_envelope(io::envelope::serialize<Schema, 3>({}).value());
        SECTION("hash algorithm") { envelope.header.hash_algorithm = static_cast<io::hash::Algorithm>(2); }
        SECTION("compression algorithm") { envelope.header.compression_algorithm = static_cast<io::envelope::CompressionAlgorithm>(3); }
        write_file(path, encode_envelope(envelope));
        check_error(io::envelope::read_header(path), Error::Code::Unsupported);
    }

    SECTION("a missing file") { check_error(io::envelope::read_header(directory.path() / "missing"), Error::Code::NotFound); }
}
