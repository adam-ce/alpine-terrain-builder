#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <expected>

namespace io::envelope {

using Bytes = std::vector<std::byte>;

inline constexpr std::size_t default_max_decompressed_size = std::size_t{1} << 30;

enum class ChecksumAlgorithm : std::uint8_t {
    None,
    HandledByCompressionLib,
};

enum class CompressionAlgorithm : std::uint8_t {
    None,
    ZstdBestCompressionWithChecksum,
};

enum class ErrorCode : std::uint8_t {
    SerializationFailed,
    DeserializationFailed,
    InvalidMagic,
    WrongClassName,
    UnsupportedClassVersion,
    UnsupportedChecksumAlgorithm,
    UnsupportedCompressionAlgorithm,
    InvalidAlgorithmCombination,
    ChecksumMismatch,
    CompressionFailed,
    DecompressionFailed,
    SizeLimitExceeded,
};

struct Error {
    ErrorCode code;
    std::errc serialization_error{};

    constexpr bool operator==(const Error &) const = default;
};

struct CompressedData {
    Bytes compressed_data;
    std::string checksum;
};

std::expected<CompressedData, Error> compress_with_checksum(
    const Bytes &uncompressed_data,
    CompressionAlgorithm compression_algorithm,
    ChecksumAlgorithm checksum_algorithm);

std::expected<Bytes, Error> checked_decompress(
    const Bytes &compressed_data,
    CompressionAlgorithm compression_algorithm,
    ChecksumAlgorithm checksum_algorithm,
    std::string_view checksum,
    std::size_t max_decompressed_size = default_max_decompressed_size);

} // namespace io::envelope
