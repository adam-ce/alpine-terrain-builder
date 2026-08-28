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
