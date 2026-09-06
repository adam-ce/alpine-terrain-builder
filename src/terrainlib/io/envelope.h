#pragma once

#include "io/bytes.h"
#include "io/compression.h"

#include <zpp_bits.h>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace io::envelope {

inline constexpr std::uint64_t magic = 0xF5FBD3EF919428CAULL;

struct Envelope {
    std::uint64_t magic;
    std::string class_name;
    std::uint32_t class_version;
    ChecksumAlgorithm checksum_algorithm;
    std::string checksum;
    CompressionAlgorithm compression_algorithm;
    std::uint64_t uncompressed_size;
    Bytes compressed_data;
};

template <std::size_t Size>
struct FixedString {
    char value[Size];

    constexpr FixedString(const char (&text)[Size]) { std::copy_n(text, Size, value); }

    constexpr auto operator<=>(const FixedString&) const = default;
};

template <std::uint32_t Number, typename VersionedPayloadType>
struct Version {
    static constexpr std::uint32_t number = Number;
    using payload_type = VersionedPayloadType;
};

namespace detail {

    template <std::uint32_t Number, typename... Versions>
    struct FindVersion;

    template <std::uint32_t Number, typename First, typename... Rest>
    struct FindVersion<Number, First, Rest...> {
        using type = std::conditional_t<First::number == Number, First, typename FindVersion<Number, Rest...>::type>;
    };

    template <std::uint32_t Number>
    struct FindVersion<Number> {
        using type = void;
    };

    template <typename... Versions>
    consteval bool versions_are_strictly_increasing()
    {
        constexpr std::uint32_t numbers[] = { Versions::number... };
        for (std::size_t index = 1; index < sizeof...(Versions); ++index) {
            if (numbers[index - 1] >= numbers[index]) {
                return false;
            }
        }
        return true;
    }

    template <typename From, typename To>
    concept ConvertsFromPrevious = requires(From previous) {
        { To::from_previous(std::move(previous)) } -> std::same_as<To>;
    };

    template <typename VersionTuple, std::size_t... Indices>
    consteval bool conversions_are_valid(std::index_sequence<Indices...>)
    {
        return (ConvertsFromPrevious<typename std::tuple_element_t<Indices, VersionTuple>::payload_type,
                    typename std::tuple_element_t<Indices + 1, VersionTuple>::payload_type>
            && ...);
    }

} // namespace detail

template <FixedString ClassName, typename... Versions>
struct PayloadSchema {
    static_assert(sizeof...(Versions) > 0, "a payload schema requires at least one version");
    static_assert(detail::versions_are_strictly_increasing<Versions...>(), "payload versions must be strictly increasing");

    using version_tuple = std::tuple<Versions...>;
    static constexpr std::size_t version_count = sizeof...(Versions);

    template <std::size_t Index>
    using version_at = std::tuple_element_t<Index, version_tuple>;

    using latest_version_descriptor = version_at<version_count - 1>;
    using latest_type = typename latest_version_descriptor::payload_type;

    static constexpr std::string_view class_name { ClassName.value, sizeof(ClassName.value) - 1 };
    static constexpr std::uint32_t latest_version = latest_version_descriptor::number;

    template <std::uint32_t Number>
    static constexpr bool supports_version = !std::is_void_v<typename detail::FindVersion<Number, Versions...>::type>;

    static constexpr bool supports_version_number(const std::uint32_t number) { return ((number == Versions::number) || ...); }

    template <std::uint32_t Number>
    using payload_type = typename detail::FindVersion<Number, Versions...>::type::payload_type;

    static_assert(version_count == 1 || detail::conversions_are_valid<version_tuple>(std::make_index_sequence<version_count - 1> {}),
        "each payload version must provide from_previous for the preceding version");
};

template <typename Schema, std::uint32_t VersionNumber>
Expected<Bytes> serialize(const typename Schema::template payload_type<VersionNumber>& payload,
    CompressionAlgorithm compression_algorithm = CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
    ChecksumAlgorithm checksum_algorithm = ChecksumAlgorithm::HandledByCompressionLib);

template <typename Schema>
Expected<Bytes> serialize(const typename Schema::latest_type& payload,
    CompressionAlgorithm compression_algorithm = CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
    ChecksumAlgorithm checksum_algorithm = ChecksumAlgorithm::HandledByCompressionLib);

template <typename Schema>
Expected<typename Schema::latest_type> deserialize(std::span<const std::byte> bytes, std::size_t max_decompressed_size = default_max_decompressed_size);

namespace detail {

template <typename Value>
Expected<Bytes> serialize_to_bytes(const Value& value)
{
    Bytes bytes;
    zpp::bits::out output(bytes, zpp::bits::alloc_limit<default_max_decompressed_size>());
    const zpp::bits::errc result = output(value);
    if (zpp::bits::failure(result)) {
        const auto cause = std::make_error_code(result.code);
        const auto code = result.code == std::errc::not_enough_memory ? Error::Code::ResourceExhausted : Error::Code::Internal;
        return Error::fail(code, "serialize envelope data", cause);
    }
    return bytes;
}

template <typename Value>
Expected<Value> deserialize_from_bytes(const std::span<const std::byte> bytes)
{
    Value value{};
    zpp::bits::in input(bytes, zpp::bits::alloc_limit<default_max_decompressed_size>());
    const zpp::bits::errc result = input(value);
    if (zpp::bits::failure(result) || input.position() != bytes.size()) {
        const auto cause = std::make_error_code(zpp::bits::failure(result) ? result.code : std::errc::bad_message);
        return Error::fail(Error::Code::CorruptData, "deserialize envelope data", cause);
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
Expected<typename Schema::latest_type> deserialize_version(
    const std::uint32_t class_version,
    const std::span<const std::byte> payload_bytes)
{
    using CurrentVersion = typename Schema::template version_at<Index>;
    if (class_version == CurrentVersion::number) {
        auto payload = deserialize_from_bytes<typename CurrentVersion::payload_type>(payload_bytes);
        if (!payload) {
            return Error::propagate(std::move(payload), "deserializing versioned envelope payload");
        }
        return convert_to_latest<Schema, Index>(std::move(*payload));
    }

    if constexpr (Index + 1 < Schema::version_count) {
        return deserialize_version<Schema, Index + 1>(class_version, payload_bytes);
    } else {
        return Error::fail(Error::Code::Unsupported,
            "unsupported envelope class version " + std::to_string(class_version));
    }
}

} // namespace detail

template <typename Schema, std::uint32_t VersionNumber>
Expected<Bytes> serialize(
    const typename Schema::template payload_type<VersionNumber> &payload,
    const CompressionAlgorithm compression_algorithm,
    const ChecksumAlgorithm checksum_algorithm)
{
    static_assert(Schema::template supports_version<VersionNumber>,
                  "the requested payload version is not part of the schema");

    auto payload_bytes = detail::serialize_to_bytes(payload);
    if (!payload_bytes) {
        return Error::propagate(std::move(payload_bytes), "serializing envelope payload");
    }

    auto compressed = compress_with_checksum(*payload_bytes, compression_algorithm, checksum_algorithm);
    if (!compressed) {
        return Error::propagate(std::move(compressed), "compressing envelope payload");
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
Expected<Bytes> serialize(
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
Expected<typename Schema::latest_type> deserialize(
    const std::span<const std::byte> bytes,
    const std::size_t max_decompressed_size)
{
    auto envelope = detail::deserialize_from_bytes<Envelope>(bytes);
    if (!envelope) {
        return Error::propagate(std::move(envelope), "reading envelope header");
    }
    if (envelope->magic != magic) {
        return Error::fail(Error::Code::CorruptData, "invalid envelope magic");
    }
    if (envelope->class_name != Schema::class_name) {
        return Error::fail(Error::Code::CorruptData,
            "unexpected envelope class \"" + envelope->class_name + "\", expected \"" + std::string(Schema::class_name) + "\"");
    }
    if (!Schema::supports_version_number(envelope->class_version)) {
        return Error::fail(Error::Code::Unsupported,
            "unsupported envelope class version " + std::to_string(envelope->class_version));
    }

    const std::size_t effective_max_decompressed_size =
        std::min(max_decompressed_size, default_max_decompressed_size);
    if (envelope->uncompressed_size > std::numeric_limits<std::size_t>::max()
        || envelope->uncompressed_size > effective_max_decompressed_size) {
        return Error::fail(Error::Code::ResourceExhausted, "envelope payload exceeds the configured size limit");
    }

    auto payload_bytes = checked_decompress(
        envelope->compressed_data,
        envelope->compression_algorithm,
        envelope->checksum_algorithm,
        envelope->checksum,
        static_cast<std::size_t>(envelope->uncompressed_size));
    if (!payload_bytes) {
        if (payload_bytes.error().code() == Error::Code::ResourceExhausted) {
            return Error::propagate(std::move(payload_bytes),
                Error::Code::CorruptData, "decompressed payload exceeds the size declared by the envelope");
        }
        if (payload_bytes.error().code() == Error::Code::InvalidInput) {
            return Error::propagate(std::move(payload_bytes),
                Error::Code::CorruptData, "envelope declares an invalid compression and checksum combination");
        }
        return Error::propagate(std::move(payload_bytes), "decompressing envelope payload");
    }
    if (payload_bytes->size() != envelope->uncompressed_size) {
        return Error::fail(Error::Code::CorruptData, "decompressed payload size does not match the envelope declaration");
    }

    return detail::deserialize_version<Schema>(envelope->class_version, *payload_bytes);
}

template <typename Schema>
Expected<typename Schema::latest_type> read_from_path(
    const std::filesystem::path& path, const std::size_t max_decompressed_size = default_max_decompressed_size)
{
    auto file_bytes = ::io::read_bytes_from_path(path);
    if (!file_bytes) {
        return Error::propagate(std::move(file_bytes), "reading envelope file \"" + path.string() + "\"");
    }
    const auto bytes = std::as_bytes(std::span { *file_bytes });
    auto result = deserialize<Schema>(bytes, max_decompressed_size);
    if (!result) {
        return Error::propagate(std::move(result), "decoding envelope file \"" + path.string() + "\"");
    }
    return std::move(*result);
}

template <typename Schema>
Expected<void> write_to_path(const typename Schema::latest_type& payload,
    const std::filesystem::path& path,
    const bool make_dirs = true,
    const CompressionAlgorithm compression_algorithm = CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
    const ChecksumAlgorithm checksum_algorithm = ChecksumAlgorithm::HandledByCompressionLib)
{
    auto serialized = serialize<Schema>(payload, compression_algorithm, checksum_algorithm);
    if (!serialized) {
        return Error::propagate(std::move(serialized), "encoding envelope file \"" + path.string() + "\"");
    }
    const auto bytes = std::span<const std::uint8_t> {
        reinterpret_cast<const std::uint8_t*>(serialized->data()),
        serialized->size(),
    };
    auto result = ::io::write_bytes_to_path(bytes, path, make_dirs);
    if (!result) {
        return Error::propagate(std::move(result), "writing envelope file \"" + path.string() + "\"");
    }
    return {};
}

} // namespace io::envelope
