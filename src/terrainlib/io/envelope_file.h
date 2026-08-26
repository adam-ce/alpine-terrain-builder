#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

#include <expected>

#include "io/bytes.h"
#include "io/envelope.h"

namespace io::envelope {

template <typename Schema>
std::expected<typename Schema::latest_type, ::Error> read_from_path(
    const std::filesystem::path& path, const std::size_t max_decompressed_size = default_max_decompressed_size)
{
    auto file_bytes = ::io::read_bytes_from_path(path);
    if (!file_bytes) {
        return std::unexpected(std::move(file_bytes.error()).with_context("reading envelope file \"" + path.string() + "\""));
    }
    const auto bytes = std::as_bytes(std::span { *file_bytes });
    auto result = deserialize<Schema>(bytes, max_decompressed_size);
    if (!result) {
        return std::unexpected(std::move(result.error()).with_context("decoding envelope file \"" + path.string() + "\""));
    }
    return std::move(*result);
}

template <typename Schema>
std::expected<void, ::Error> write_to_path(const typename Schema::latest_type& payload,
    const std::filesystem::path& path,
    const bool make_dirs = true,
    const CompressionAlgorithm compression_algorithm = CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
    const ChecksumAlgorithm checksum_algorithm = ChecksumAlgorithm::HandledByCompressionLib)
{
    auto serialized = serialize<Schema>(payload, compression_algorithm, checksum_algorithm);
    if (!serialized) {
        return std::unexpected(std::move(serialized.error()).with_context("encoding envelope file \"" + path.string() + "\""));
    }
    const auto bytes = std::span<const std::uint8_t> {
        reinterpret_cast<const std::uint8_t*>(serialized->data()),
        serialized->size(),
    };
    auto result = ::io::write_bytes_to_path(bytes, path, make_dirs);
    if (!result) {
        return std::unexpected(std::move(result.error()).with_context("writing envelope file \"" + path.string() + "\""));
    }
    return {};
}

} // namespace io::envelope
