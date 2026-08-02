#include "../catch2_helpers.h"

#include "io/envelope.h"

#include <zpp_bits.h>

namespace {

namespace v1 {

struct Payload {
    std::uint32_t id;
    std::string name;

    bool operator==(const Payload &) const = default;
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

    bool operator==(const Payload &) const = default;
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

    bool operator==(const Payload &) const = default;
};

} // namespace v3

using Schema = io::envelope::PayloadSchema<
    "test.Payload",
    io::envelope::Version<1, v1::Payload>,
    io::envelope::Version<2, v2::Payload>,
    io::envelope::Version<3, v3::Payload>>;

static_assert(std::is_aggregate_v<v1::Payload>);
static_assert(std::is_aggregate_v<v2::Payload>);
static_assert(std::is_aggregate_v<v3::Payload>);
static_assert(Schema::class_name == "test.Payload");
static_assert(Schema::latest_version == 3);
static_assert(std::same_as<Schema::latest_type, v3::Payload>);
static_assert(std::same_as<Schema::payload_type<1>, v1::Payload>);

io::envelope::Bytes encode_envelope(const io::envelope::Envelope &envelope)
{
    io::envelope::Bytes bytes;
    zpp::bits::out output(bytes);
    output(envelope).or_throw();
    return bytes;
}

io::envelope::Envelope decode_envelope(const io::envelope::Bytes &bytes)
{
    io::envelope::Envelope envelope{};
    zpp::bits::in input(bytes);
    input(envelope).or_throw();
    return envelope;
}

template <typename Result>
void check_error(const Result &result, const io::envelope::ErrorCode expected)
{
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == expected);
}

} // namespace

TEST_CASE("Envelope round trips the latest payload version")
{
    const v3::Payload expected{
        .id = 42,
        .label = "latest",
        .enabled = false,
        .samples = {1, 2, 3},
    };

    const auto bytes = io::envelope::serialize<Schema, 3>(expected);
    REQUIRE(bytes.has_value());

    const auto envelope = decode_envelope(*bytes);
    CHECK(envelope.magic == io::envelope::magic);
    CHECK(envelope.class_name == Schema::class_name);
    CHECK(envelope.class_version == 3);
    CHECK(envelope.checksum_algorithm == io::envelope::ChecksumAlgorithm::HandledByCompressionLib);
    CHECK(envelope.checksum.empty());
    CHECK(envelope.compression_algorithm
          == io::envelope::CompressionAlgorithm::ZstdBestCompressionWithChecksum);

    const auto result = io::envelope::deserialize<Schema>(*bytes);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("Envelope upgrades older payload versions")
{
    SECTION("version 1 is upgraded through every subsequent version")
    {
        const v1::Payload original{.id = 7, .name = "version one"};
        const auto bytes = io::envelope::serialize<Schema, 1>(original);
        REQUIRE(bytes.has_value());

        const auto result = io::envelope::deserialize<Schema>(*bytes);
        REQUIRE(result.has_value());
        CHECK(*result == v3::Payload{
                             .id = 7,
                             .label = "version one",
                             .enabled = true,
                             .samples = {},
                         });
    }

    SECTION("version 2 is upgraded to the latest version")
    {
        const v2::Payload original{.id = 9, .name = "version two", .enabled = false};
        const auto bytes = io::envelope::serialize<Schema, 2>(original);
        REQUIRE(bytes.has_value());

        const auto result = io::envelope::deserialize<Schema>(*bytes);
        REQUIRE(result.has_value());
        CHECK(*result == v3::Payload{
                             .id = 9,
                             .label = "version two",
                             .enabled = false,
                             .samples = {},
                         });
    }
}

TEST_CASE("Envelope supports uncompressed data without a checksum")
{
    const v3::Payload expected{
        .id = 11,
        .label = "plain",
        .enabled = true,
        .samples = {5, 8},
    };
    const auto bytes = io::envelope::serialize<Schema, 3>(
        expected,
        io::envelope::CompressionAlgorithm::None,
        io::envelope::ChecksumAlgorithm::None);
    REQUIRE(bytes.has_value());

    const auto envelope = decode_envelope(*bytes);
    CHECK(envelope.compression_algorithm == io::envelope::CompressionAlgorithm::None);
    CHECK(envelope.checksum_algorithm == io::envelope::ChecksumAlgorithm::None);
    CHECK(envelope.checksum.empty());

    const auto result = io::envelope::deserialize<Schema>(*bytes);
    REQUIRE(result.has_value());
    CHECK(*result == expected);
}

TEST_CASE("Checked compression round trips and validates its checksum")
{
    const io::envelope::Bytes original(4096, std::byte{0x2a});
    const auto compressed = io::envelope::compress_with_checksum(
        original,
        io::envelope::CompressionAlgorithm::ZstdBestCompressionWithChecksum,
        io::envelope::ChecksumAlgorithm::HandledByCompressionLib);
    REQUIRE(compressed.has_value());
    CHECK(compressed->compressed_data.size() < original.size());
    CHECK(compressed->checksum.empty());

    const auto result = io::envelope::checked_decompress(
        compressed->compressed_data,
        io::envelope::CompressionAlgorithm::ZstdBestCompressionWithChecksum,
        io::envelope::ChecksumAlgorithm::HandledByCompressionLib,
        compressed->checksum);
    REQUIRE(result.has_value());
    CHECK(*result == original);

    auto corrupted = compressed->compressed_data;
    corrupted.back() ^= std::byte{0x01};
    check_error(
        io::envelope::checked_decompress(
            corrupted,
            io::envelope::CompressionAlgorithm::ZstdBestCompressionWithChecksum,
            io::envelope::ChecksumAlgorithm::HandledByCompressionLib,
            {}),
        io::envelope::ErrorCode::ChecksumMismatch);

    check_error(
        io::envelope::checked_decompress(
            compressed->compressed_data,
            io::envelope::CompressionAlgorithm::ZstdBestCompressionWithChecksum,
            io::envelope::ChecksumAlgorithm::HandledByCompressionLib,
            {},
            original.size() - 1),
        io::envelope::ErrorCode::SizeLimitExceeded);
}

TEST_CASE("Envelope rejects incompatible metadata")
{
    const v3::Payload payload{.id = 1, .label = "metadata", .enabled = true, .samples = {}};
    const auto serialized = io::envelope::serialize<Schema, 3>(payload);
    REQUIRE(serialized.has_value());

    SECTION("magic")
    {
        auto envelope = decode_envelope(*serialized);
        ++envelope.magic;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::InvalidMagic);
    }

    SECTION("class name")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.class_name = "other.Payload";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::WrongClassName);
    }

    SECTION("class version")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.class_version = 99;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::UnsupportedClassVersion);
    }

    SECTION("checksum algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum_algorithm = static_cast<io::envelope::ChecksumAlgorithm>(99);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::UnsupportedChecksumAlgorithm);
    }

    SECTION("compression algorithm")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.compression_algorithm = static_cast<io::envelope::CompressionAlgorithm>(99);
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::UnsupportedCompressionAlgorithm);
    }

    SECTION("algorithm combination")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum_algorithm = io::envelope::ChecksumAlgorithm::None;
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::InvalidAlgorithmCombination);
    }

    SECTION("external checksum")
    {
        auto envelope = decode_envelope(*serialized);
        envelope.checksum = "not used by zstd";
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::InvalidAlgorithmCombination);
    }
}

TEST_CASE("Envelope reports malformed serialized data")
{
    SECTION("envelope")
    {
        const io::envelope::Bytes malformed{std::byte{0x01}, std::byte{0x02}};
        check_error(io::envelope::deserialize<Schema>(malformed),
                    io::envelope::ErrorCode::DeserializationFailed);
    }

    SECTION("payload")
    {
        const io::envelope::Envelope envelope{
            .magic = io::envelope::magic,
            .class_name = std::string{Schema::class_name},
            .class_version = 3,
            .checksum_algorithm = io::envelope::ChecksumAlgorithm::None,
            .checksum = {},
            .compression_algorithm = io::envelope::CompressionAlgorithm::None,
            .compressed_data = {std::byte{0x01}},
        };
        check_error(io::envelope::deserialize<Schema>(encode_envelope(envelope)),
                    io::envelope::ErrorCode::DeserializationFailed);
    }
}
