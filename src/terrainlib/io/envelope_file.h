#pragma once

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <type_traits>
#include <variant>

#include <expected>

#include "io/bytes.h"
#include "io/envelope.h"

namespace io::envelope {

using FileError = std::variant<::io::Error, Error>;

inline bool is_file_not_found(const FileError& error)
{
    const auto* io_error = std::get_if<::io::Error>(&error);
    return io_error != nullptr && *io_error == ::io::Error(::io::Error::OpenFile);
}

inline std::string describe_error(const Error& error) { return "envelope error " + std::to_string(static_cast<unsigned>(error.code)); }

inline std::string describe_error(const FileError& error)
{
    return std::visit(
        [](const auto& value) -> std::string {
            using Value = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<Value, ::io::Error>) {
                return value.to_string();
            } else {
                return describe_error(value);
            }
        },
        error);
}

template <typename Schema>
std::expected<typename Schema::latest_type, FileError> read_from_path(
    const std::filesystem::path& path, const std::size_t max_decompressed_size = default_max_decompressed_size)
{
    auto file_bytes = ::io::read_bytes_from_path(path);
    if (!file_bytes) {
        return std::unexpected(FileError { file_bytes.error() });
    }
    const auto bytes = std::as_bytes(std::span { *file_bytes });
    auto result = deserialize<Schema>(bytes, max_decompressed_size);
    if (!result) {
        return std::unexpected(FileError { result.error() });
    }
    return std::move(*result);
}

template <typename Schema>
std::expected<void, FileError> write_to_path(const typename Schema::latest_type& payload,
    const std::filesystem::path& path,
    const bool make_dirs = true,
    const CompressionAlgorithm compression_algorithm = CompressionAlgorithm::ZstdDefaultCompressionWithChecksum,
    const ChecksumAlgorithm checksum_algorithm = ChecksumAlgorithm::HandledByCompressionLib)
{
    auto serialized = serialize<Schema>(payload, compression_algorithm, checksum_algorithm);
    if (!serialized) {
        return std::unexpected(FileError { serialized.error() });
    }
    const auto bytes = std::span<const std::uint8_t> {
        reinterpret_cast<const std::uint8_t*>(serialized->data()),
        serialized->size(),
    };
    auto result = ::io::write_bytes_to_path(bytes, path, make_dirs);
    if (!result) {
        return std::unexpected(FileError { result.error() });
    }
    return {};
}

} // namespace io::envelope
