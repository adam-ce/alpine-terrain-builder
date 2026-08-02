#pragma once

#include "io/compression.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
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
    Bytes compressed_data;
};

template <std::size_t Size>
struct FixedString {
    char value[Size];

    constexpr FixedString(const char (&text)[Size])
    {
        std::copy_n(text, Size, value);
    }

    constexpr auto operator<=>(const FixedString &) const = default;
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
    constexpr std::uint32_t numbers[] = {Versions::number...};
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
    return (ConvertsFromPrevious<
                typename std::tuple_element_t<Indices, VersionTuple>::payload_type,
                typename std::tuple_element_t<Indices + 1, VersionTuple>::payload_type>
            && ...);
}

} // namespace detail

template <FixedString ClassName, typename... Versions>
struct PayloadSchema {
    static_assert(sizeof...(Versions) > 0, "a payload schema requires at least one version");
    static_assert(detail::versions_are_strictly_increasing<Versions...>(),
                  "payload versions must be strictly increasing");

    using version_tuple = std::tuple<Versions...>;
    static constexpr std::size_t version_count = sizeof...(Versions);

    template <std::size_t Index>
    using version_at = std::tuple_element_t<Index, version_tuple>;

    using latest_version_descriptor = version_at<version_count - 1>;
    using latest_type = typename latest_version_descriptor::payload_type;

    static constexpr std::string_view class_name{ClassName.value, sizeof(ClassName.value) - 1};
    static constexpr std::uint32_t latest_version = latest_version_descriptor::number;

    template <std::uint32_t Number>
    static constexpr bool supports_version =
        !std::is_void_v<typename detail::FindVersion<Number, Versions...>::type>;

    static constexpr bool supports_version_number(const std::uint32_t number)
    {
        return ((number == Versions::number) || ...);
    }

    template <std::uint32_t Number>
    using payload_type = typename detail::FindVersion<Number, Versions...>::type::payload_type;

    static_assert(version_count == 1
                      || detail::conversions_are_valid<version_tuple>(
                          std::make_index_sequence<version_count - 1>{}),
                  "each payload version must provide from_previous for the preceding version");
};

template <typename Schema, std::uint32_t VersionNumber>
std::expected<Bytes, Error> serialize(
    const typename Schema::template payload_type<VersionNumber> &payload,
    CompressionAlgorithm compression_algorithm = CompressionAlgorithm::ZstdBestCompressionWithChecksum,
    ChecksumAlgorithm checksum_algorithm = ChecksumAlgorithm::HandledByCompressionLib);

template <typename Schema>
std::expected<typename Schema::latest_type, Error> deserialize(
    std::span<const std::byte> bytes,
    std::size_t max_decompressed_size = default_max_decompressed_size);

} // namespace io::envelope

#include "io/envelope.inl"
