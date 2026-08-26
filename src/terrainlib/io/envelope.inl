#pragma once

#include <zpp_bits.h>

#include <limits>

namespace io::envelope {
namespace detail {

template <typename Value>
std::expected<Bytes, ::Error> serialize_to_bytes(const Value& value)
{
    Bytes bytes;
    zpp::bits::out output(bytes, zpp::bits::alloc_limit<default_max_decompressed_size>());
    const zpp::bits::errc result = output(value);
    if (zpp::bits::failure(result)) {
        const auto cause = std::make_error_code(result.code);
        const auto code = result.code == std::errc::not_enough_memory ? ::Error::Code::ResourceExhausted : ::Error::Code::Internal;
        return std::unexpected(::Error::make(code, "serialize envelope data", cause));
    }
    return bytes;
}

template <typename Value>
std::expected<Value, ::Error> deserialize_from_bytes(const std::span<const std::byte> bytes)
{
    Value value{};
    zpp::bits::in input(bytes, zpp::bits::alloc_limit<default_max_decompressed_size>());
    const zpp::bits::errc result = input(value);
    if (zpp::bits::failure(result) || input.position() != bytes.size()) {
        const auto cause = std::make_error_code(zpp::bits::failure(result) ? result.code : std::errc::bad_message);
        return std::unexpected(::Error::make(::Error::Code::CorruptData, "deserialize envelope data", cause));
    }
    return value;
}

template <typename Schema, std::size_t Index, typename Current>
typename Schema::latest_type convert_to_latest(Current current)
{
    if constexpr (Index + 1 == Schema::version_count) {
        return current;
    } else {
        using Next = typename Schema::template version_at<Index + 1>::payload_type;
        return convert_to_latest<Schema, Index + 1>(Next::from_previous(std::move(current)));
    }
}

template <typename Schema, std::size_t Index = 0>
std::expected<typename Schema::latest_type, ::Error> deserialize_version(
    const std::uint32_t class_version,
    const std::span<const std::byte> payload_bytes)
{
    using CurrentVersion = typename Schema::template version_at<Index>;
    if (class_version == CurrentVersion::number) {
        auto payload = deserialize_from_bytes<typename CurrentVersion::payload_type>(payload_bytes);
        if (!payload) {
            return std::unexpected(std::move(payload.error()).with_context("deserializing versioned envelope payload"));
        }
        return convert_to_latest<Schema, Index>(std::move(*payload));
    }

    if constexpr (Index + 1 < Schema::version_count) {
        return deserialize_version<Schema, Index + 1>(class_version, payload_bytes);
    } else {
        return std::unexpected(::Error::make(::Error::Code::Unsupported,
            "unsupported envelope class version " + std::to_string(class_version)));
    }
}

} // namespace detail

template <typename Schema, std::uint32_t VersionNumber>
std::expected<Bytes, ::Error> serialize(
    const typename Schema::template payload_type<VersionNumber> &payload,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm)
{
    static_assert(Schema::template supports_version<VersionNumber>,
                  "the requested payload version is not part of the schema");

    auto payload_bytes = detail::serialize_to_bytes(payload);
    if (!payload_bytes) {
        return std::unexpected(std::move(payload_bytes.error()).with_context("serializing envelope payload"));
    }

    auto compressed = compress_with_checksum(*payload_bytes, compression_algorithm, checksum_algorithm);
    if (!compressed) {
        return std::unexpected(std::move(compressed.error()).with_context("compressing envelope payload"));
    }

    const Envelope envelope{
        .magic = magic,
        .class_name = std::string{Schema::class_name},
        .class_version = VersionNumber,
        .checksum_algorithm = checksum_algorithm,
        .checksum = std::move(compressed->checksum),
        .compression_algorithm = compression_algorithm,
        .uncompressed_size = payload_bytes->size(),
        .compressed_data = std::move(compressed->compressed_data),
    };
    return detail::serialize_to_bytes(envelope);
}

template <typename Schema>
std::expected<Bytes, ::Error> serialize(
    const typename Schema::latest_type &payload,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm)
{
    return serialize<Schema, Schema::latest_version>(
        payload,
        compression_algorithm,
        checksum_algorithm);
}

template <typename Schema>
std::expected<typename Schema::latest_type, ::Error> deserialize(
    const std::span<const std::byte> bytes,
    const std::size_t max_decompressed_size)
{
    auto envelope = detail::deserialize_from_bytes<Envelope>(bytes);
    if (!envelope) {
        return std::unexpected(std::move(envelope.error()).with_context("reading envelope header"));
    }
    if (envelope->magic != magic) {
        return std::unexpected(::Error::make(::Error::Code::CorruptData, "invalid envelope magic"));
    }
    if (envelope->class_name != Schema::class_name) {
        return std::unexpected(::Error::make(::Error::Code::CorruptData,
            "unexpected envelope class \"" + envelope->class_name + "\", expected \"" + std::string(Schema::class_name) + "\""));
    }
    if (!Schema::supports_version_number(envelope->class_version)) {
        return std::unexpected(::Error::make(::Error::Code::Unsupported,
            "unsupported envelope class version " + std::to_string(envelope->class_version)));
    }

    const std::size_t effective_max_decompressed_size =
        std::min(max_decompressed_size, default_max_decompressed_size);
    if (envelope->uncompressed_size > std::numeric_limits<std::size_t>::max()
        || envelope->uncompressed_size > effective_max_decompressed_size) {
        return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "envelope payload exceeds the configured size limit"));
    }

    auto payload_bytes = checked_decompress(
        envelope->compressed_data,
        envelope->compression_algorithm,
        envelope->checksum_algorithm,
        envelope->checksum,
        static_cast<std::size_t>(envelope->uncompressed_size));
    if (!payload_bytes) {
        if (payload_bytes.error().code() == ::Error::Code::ResourceExhausted) {
            return std::unexpected(std::move(payload_bytes.error()).reclassified(
                ::Error::Code::CorruptData, "decompressed payload exceeds the size declared by the envelope"));
        }
        if (payload_bytes.error().code() == ::Error::Code::InvalidInput) {
            return std::unexpected(std::move(payload_bytes.error()).reclassified(
                ::Error::Code::CorruptData, "envelope declares an invalid compression and checksum combination"));
        }
        return std::unexpected(std::move(payload_bytes.error()).with_context("decompressing envelope payload"));
    }
    if (payload_bytes->size() != envelope->uncompressed_size) {
        return std::unexpected(::Error::make(::Error::Code::CorruptData, "decompressed payload size does not match the envelope declaration"));
    }

    return detail::deserialize_version<Schema>(envelope->class_version, *payload_bytes);
}

} // namespace io::envelope
