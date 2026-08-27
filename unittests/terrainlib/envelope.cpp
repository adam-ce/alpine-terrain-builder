#include "../catch2_helpers.h"

#include "io/envelope.h"

#include <zpp_bits.h>
#include <zstd.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string_view>

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
    io::envelope::CompressionAlgorithm::ZstdBestCompressionWithChecksum,
    io::envelope::CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
};

static_assert(std::is_aggregate_v<v1::Payload>);
static_assert(std::is_aggregate_v<v2::Payload>);
static_assert(std::is_aggregate_v<v3::Payload>);
static_assert(Schema::class_name == "test.Payload");
static_assert(Schema::latest_version == 3);
static_assert(std::same_as<Schema::latest_type, v3::Payload>);
static_assert(std::same_as<Schema::payload_type<1>, v1::Payload>);

template <typename Value>
io::envelope::Bytes encode_value(const Value& value)
{
    io::envelope::Bytes bytes;
    zpp::bits::out output(bytes);
    output(value).or_throw();
    return bytes;
}

io::envelope::Bytes encode_envelope(const io::envelope::Envelope& envelope) { return encode_value(envelope); }

io::envelope::Envelope decode_envelope(const io::envelope::Bytes& bytes)
{
    io::envelope::Envelope envelope {};
    zpp::bits::in input(bytes);
    input(envelope).or_throw();
    return envelope;
}

io::envelope::Bytes compress_without_content_size(const io::envelope::Bytes& uncompressed_data)
{
    const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context { ZSTD_createCCtx(), &ZSTD_freeCCtx };
    if (!context || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, 0))
        || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1))) {
        throw std::runtime_error { "could not configure zstd test context" };
    }

    io::envelope::Bytes compressed_data(ZSTD_compressBound(uncompressed_data.size()));
    const std::size_t compressed_size
        = ZSTD_compress2(context.get(), compressed_data.data(), compressed_data.size(), uncompressed_data.data(), uncompressed_data.size());
    if (ZSTD_isError(compressed_size)) {
        throw std::runtime_error { "could not create zstd test data" };
    }
    compressed_data.resize(compressed_size);
    return compressed_data;
}

template <typename Result>
void check_error(const Result& result, const ::Error::Code expected)
{
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == expected);
}

io::envelope::Bytes bytes_from_string(const std::string_view text)
{
    io::envelope::Bytes bytes;
    bytes.reserve(text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return bytes;
}

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
    CHECK(envelope.magic == io::envelope::magic);
    CHECK(envelope.class_name == Schema::class_name);
    CHECK(envelope.class_version == 3);
    CHECK(envelope.checksum_algorithm == io::envelope::ChecksumAlgorithm::HandledByCompressionLib);
    CHECK(envelope.checksum.empty());
    CHECK(envelope.compression_algorithm == io::envelope::CompressionAlgorithm::ZstdDefaultCompressionWithChecksum);
    CHECK(envelope.uncompressed_size == encode_value(expected).size());

    const auto result = io::envelope::deserialize<Schema>(*bytes);
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

TEST_CASE("Envelope supports uncompressed data without a checksum")
{
    const v3::Payload expected {
        .id = 11,
        .label = "plain",
        .enabled = true,
        .samples = { 5, 8 },
    };
    const auto bytes = io::envelope::serialize<Schema, 3>(expected, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::None);
    REQUIRE(bytes.has_value());

    const auto envelope = decode_envelope(*bytes);
    CHECK(envelope.compression_algorithm == io::envelope::CompressionAlgorithm::None);
    CHECK(envelope.checksum_algorithm == io::envelope::ChecksumAlgorithm::None);
    CHECK(envelope.checksum.empty());
    CHECK(envelope.uncompressed_size == envelope.compressed_data.size());

    const auto result = io::envelope::deserialize<Schema>(*bytes);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("CRC-32C uses its canonical hexadecimal representation")
{
    SECTION("standard test vector")
    {
        const auto compressed = io::envelope::compress_with_checksum(
            bytes_from_string("123456789"), io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c);
        REQUIRE(compressed.has_value());
        CHECK(compressed->checksum == "e3069283");
    }

    SECTION("empty input")
    {
        const auto compressed = io::envelope::compress_with_checksum({}, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c);
        REQUIRE(compressed.has_value());
        CHECK(compressed->checksum == "00000000");
    }
}

TEST_CASE("CRC-32C protects independently compressed data")
{
    const auto original = bytes_from_string("payload protected by CRC-32C");

    SECTION("without compression")
    {
        const auto compressed
            = io::envelope::compress_with_checksum(original, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c);
        REQUIRE(compressed.has_value());

        const auto result = io::envelope::checked_decompress(
            compressed->compressed_data, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c, compressed->checksum);
        REQUIRE(result.has_value());
        CHECK(*result == original);

        auto corrupted = compressed->compressed_data;
        corrupted.front() ^= std::byte { 0x01 };
        check_error(io::envelope::checked_decompress(
                        corrupted, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c, compressed->checksum),
            ::Error::Code::CorruptData);
    }

    SECTION("with zstd compression")
    {
        for (const auto compression_algorithm : zstd_compression_algorithms) {
            CAPTURE(compression_algorithm);
            const auto compressed = io::envelope::compress_with_checksum(original, compression_algorithm, io::envelope::ChecksumAlgorithm::Crc32c);
            REQUIRE(compressed.has_value());
            REQUIRE(compressed->checksum.size() == 8);

            const auto result = io::envelope::checked_decompress(
                compressed->compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::Crc32c, compressed->checksum);
            REQUIRE(result.has_value());
            CHECK(*result == original);

            auto wrong_checksum = compressed->checksum;
            wrong_checksum.front() = wrong_checksum.front() == '0' ? '1' : '0';
            check_error(
                io::envelope::checked_decompress(compressed->compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::Crc32c, wrong_checksum),
                ::Error::Code::CorruptData);
        }
    }

    SECTION("malformed checksum")
    {
        check_error(io::envelope::checked_decompress(original, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c, "E3069283"),
            ::Error::Code::CorruptData);
        check_error(io::envelope::checked_decompress(original, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c, {}),
            ::Error::Code::CorruptData);
    }
}

TEST_CASE("Envelope round trips and upgrades payloads protected by CRC-32C")
{
    SECTION("latest version")
    {
        const v3::Payload expected {
            .id = 12,
            .label = "crc",
            .enabled = true,
            .samples = { 3, 5, 8 },
        };
        const auto bytes = io::envelope::serialize<Schema, 3>(expected, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c);
        REQUIRE(bytes.has_value());

        const auto envelope = decode_envelope(*bytes);
        CHECK(envelope.checksum_algorithm == io::envelope::ChecksumAlgorithm::Crc32c);
        CHECK(envelope.checksum.size() == 8);
        CHECK(envelope.compression_algorithm == io::envelope::CompressionAlgorithm::None);

        const auto result = io::envelope::deserialize<Schema>(*bytes);
        REQUIRE(result.has_value());
        CHECK(*result == expected);

        auto corrupted = envelope;
        corrupted.compressed_data.front() ^= std::byte { 0x01 };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(corrupted)), ::Error::Code::CorruptData);
    }

    SECTION("older version")
    {
        const v1::Payload original { .id = 13, .name = "crc version one" };
        const auto bytes = io::envelope::serialize<Schema, 1>(original, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::Crc32c);
        REQUIRE(bytes.has_value());

        const auto result = io::envelope::deserialize<Schema>(*bytes);
        REQUIRE(result.has_value());
        CHECK(*result
            == v3::Payload {
                .id = 13,
                .label = "crc version one",
                .enabled = true,
                .samples = {},
            });
    }
}

TEST_CASE("Checked compression round trips and validates its checksum")
{
    const io::envelope::Bytes original(4096, std::byte { 0x2a });
    for (const auto compression_algorithm : zstd_compression_algorithms) {
        CAPTURE(compression_algorithm);
        const auto compressed = io::envelope::compress_with_checksum(original, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib);
        REQUIRE(compressed.has_value());
        CHECK(compressed->compressed_data.size() < original.size());
        CHECK(compressed->checksum.empty());

        const auto result = io::envelope::checked_decompress(
            compressed->compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib, compressed->checksum);
        REQUIRE(result.has_value());
        CHECK(*result == original);

        const auto empty_compressed = io::envelope::compress_with_checksum({}, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib);
        REQUIRE(empty_compressed.has_value());
        const auto empty_result = io::envelope::checked_decompress(
            empty_compressed->compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib, empty_compressed->checksum);
        REQUIRE(empty_result.has_value());
        CHECK(empty_result->empty());

        auto corrupted = compressed->compressed_data;
        corrupted.back() ^= std::byte { 0x01 };
        check_error(io::envelope::checked_decompress(corrupted, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib, {}),
            ::Error::Code::CorruptData);

        check_error(io::envelope::checked_decompress(
                        compressed->compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib, {}, original.size() - 1),
            ::Error::Code::ResourceExhausted);
    }
}

TEST_CASE("Checked decompression uses its maximum when the format omits the content size")
{
    const io::envelope::Bytes original(4096, std::byte { 0x37 });
    const auto compressed_data = compress_without_content_size(original);

    for (const auto compression_algorithm : zstd_compression_algorithms) {
        CAPTURE(compression_algorithm);
        const auto result = io::envelope::checked_decompress(
            compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib, {}, original.size());
        REQUIRE(result.has_value());
        CHECK(*result == original);

        check_error(io::envelope::checked_decompress(
                        compressed_data, compression_algorithm, io::envelope::ChecksumAlgorithm::HandledByCompressionLib, {}, original.size() - 1),
            ::Error::Code::ResourceExhausted);
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
        ++envelope.magic;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("class name")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.class_name = "other.Payload";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("class version")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.class_version = 99;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::Unsupported);
    }

    SECTION("checksum algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum_algorithm = static_cast<io::envelope::ChecksumAlgorithm>(99);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::Unsupported);
    }

    SECTION("compression algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.compression_algorithm = static_cast<io::envelope::CompressionAlgorithm>(99);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::Unsupported);
    }

    SECTION("algorithm combination")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum_algorithm = io::envelope::ChecksumAlgorithm::None;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("external checksum")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum = "not used by zstd";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("malformed CRC-32C checksum")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum_algorithm = io::envelope::ChecksumAlgorithm::Crc32c;
        envelope.checksum = "1234";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("incorrect CRC-32C checksum")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum_algorithm = io::envelope::ChecksumAlgorithm::Crc32c;
        envelope.checksum = "00000000";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("uncompressed size is smaller than the payload")
    {
        auto envelope = decode_envelope(*serialized);
        --envelope.uncompressed_size;
        const auto result = io::envelope::deserialize<Schema>(encode_envelope(envelope));
        check_error(result, ::Error::Code::CorruptData);
        CHECK(result.error().to_string().find("reclassified ResourceExhausted -> CorruptData") != std::string::npos);
    }

    SECTION("uncompressed size is larger than the payload")
    {
        auto envelope = decode_envelope(*serialized);
        ++envelope.uncompressed_size;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("zero uncompressed size does not mean unspecified")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.uncompressed_size = 0;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("uncompressed size exceeds the hard limit")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.uncompressed_size = io::envelope::default_max_decompressed_size + 1;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::ResourceExhausted);
    }

    SECTION("uncompressed size exceeds a caller limit")
    {
        auto envelope = decode_envelope(*serialized);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope), static_cast<std::size_t>(envelope.uncompressed_size - 1)),
            ::Error::Code::ResourceExhausted);
    }
}

TEST_CASE("Envelope validates the size of uncompressed data")
{
    const v3::Payload payload { .id = 2, .label = "plain", .enabled = true, .samples = { 1 } };
    const auto serialized = io::envelope::serialize<Schema, 3>(payload, io::envelope::CompressionAlgorithm::None, io::envelope::ChecksumAlgorithm::None);
    REQUIRE(serialized.has_value());

    SECTION("declared size is smaller")
    {
        auto envelope = decode_envelope(*serialized);
        --envelope.uncompressed_size;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }

    SECTION("declared size is larger")
    {
        auto envelope = decode_envelope(*serialized);
        ++envelope.uncompressed_size;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }
}

TEST_CASE("Envelope reports malformed serialized data")
{
    SECTION("envelope")
    {
        const io::envelope::Bytes malformed { std::byte { 0x01 }, std::byte { 0x02 } };
        check_error(io::envelope::deserialize<Schema>(malformed), ::Error::Code::CorruptData);
    }

    SECTION("payload")
    {
        const io::envelope::Envelope envelope {
            .magic = io::envelope::magic,
            .class_name = std::string { Schema::class_name },
            .class_version = 3,
            .checksum_algorithm = io::envelope::ChecksumAlgorithm::None,
            .checksum = {},
            .compression_algorithm = io::envelope::CompressionAlgorithm::None,
            .uncompressed_size = 1,
            .compressed_data = { std::byte { 0x01 } },
        };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)), ::Error::Code::CorruptData);
    }
}
