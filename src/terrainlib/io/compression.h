#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <expected>

#include "Error.h"

namespace io::envelope {

inline constexpr std::size_t default_max_decompressed_size = std::size_t { 1 } << 30;

enum class CompressionAlgorithm : std::uint8_t {
    None,
    ZstdBestCompression,
    ZstdDefaultCompression,
};

// The algorithm must be an enumerator. Zstandard frames are written without their frame checksum.
Expected<std::vector<std::byte>> compress(std::span<const std::byte> uncompressed_data, CompressionAlgorithm compression_algorithm);

// The algorithm must be an enumerator. Zstandard frames are accepted with or without their frame checksum.
Expected<std::vector<std::byte>> decompress(
    std::span<const std::byte> compressed_data, CompressionAlgorithm compression_algorithm, std::size_t max_decompressed_size = default_max_decompressed_size);

} // namespace io::envelope
