#pragma once

#include <zpp_bits.h>

#include "io/bytes.h"
#include "io/utils.h"
#include "type_utils.h"

namespace io {

template <typename T>
tl::expected<std::vector<uint8_t>, Error> write_to_bytes(const T &value) {
    std::vector<uint8_t> data;
    zpp::bits::out out(data);
    const auto result = out(value);
    if (zpp::bits::failure(result)) {
        std::error_code error_code = std::make_error_code(result);
        LOG_ERROR("error while writing {}: {}", type_name<T>(), error_code.message());

        switch (result) {
        case std::errc::no_buffer_space:     // growing buffer would grow beyond the allocation limits or overflow.
        case std::errc::message_size:        // message size is beyond the user defined allocation limits.
        case std::errc::result_out_of_range: // attempting to write or read from a too short buffer.
            return tl::unexpected(Error::OutOfMemory);
        case std::errc::value_too_large:  // varint (variable length integer) encoding is beyond the representation limits.
        case std::errc::bad_message:      // attempt to read a variant of unrecognized type.
        case std::errc::invalid_argument: // attempting to serialize null pointer or a value-less variant.
            return tl::unexpected(Error::Serialize);
        case std::errc::protocol_error: // attempt to deserialize an invalid protocol message.
        case std::errc::not_supported:  // attempt to call an RPC that is not listed as supported.
            UNREACHABLE();
        default:
            UNREACHABLE();
        }
    }
    return data;
}

template <typename T>
tl::expected<T, Error> read_from_bytes(const std::span<const uint8_t> bytes) {
    zpp::bits::in in(bytes);
    T value;
    const auto result = in(value);
    if (zpp::bits::failure(result)) {
        std::error_code error_code = std::make_error_code(result);
        LOG_ERROR("error while reading {}: {}", type_name<T>(), error_code.message());

        switch (result) {
        case std::errc::no_buffer_space:     // growing buffer would grow beyond the allocation limits or overflow.
        case std::errc::message_size:        // message size is beyond the user defined allocation limits.
        case std::errc::result_out_of_range: // attempting to write or read from a too short buffer.
            return tl::unexpected(Error::OutOfMemory);
        case std::errc::value_too_large:  // varint (variable length integer) encoding is beyond the representation limits.
        case std::errc::bad_message:      // attempt to read a variant of unrecognized type.
        case std::errc::invalid_argument: // attempting to serialize null pointer or a value-less variant.
            return tl::unexpected(Error::Deserialize);
        case std::errc::protocol_error: // attempt to deserialize an invalid protocol message.
        case std::errc::not_supported:  // attempt to call an RPC that is not listed as supported.
            UNREACHABLE();
        default:
            UNREACHABLE();
        }
    }
    return value;
}

template <typename T>
tl::expected<T, Error> read_from_path(const std::filesystem::path &path) {
    const auto result = read_bytes_from_path(path);
    if (!result.has_value()) {
        return tl::unexpected(result.error());
    }
    const std::vector<uint8_t> bytes = result.value();
    return read_from_bytes<T>(bytes);
}

template <typename T>
tl::expected<void, Error> write_to_path(const T &value, const std::filesystem::path &path, bool make_dirs) {
    const auto result = write_to_bytes(value);
    if (!result.has_value()) {
        return tl::unexpected(result.error());
    }

    const std::vector<uint8_t> bytes = result.value();
    return write_bytes_to_path(bytes, path, make_dirs);
}

} // namespace io
