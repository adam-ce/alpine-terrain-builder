#include "io/envelope.h"

#include <magic_enum/magic_enum.hpp>

namespace io::envelope {

Expected<Header> read_header(const std::filesystem::path& path)
{
    auto file_bytes = ::io::read_bytes_from_path(path, max_header_size);
    if (!file_bytes) {
        return Error::propagate(std::move(file_bytes), "reading envelope file \"" + path.string() + "\"");
    }
    // zpp_bits resizes containers before checking their size against the input, so the limit bounds allocations.
    Header header {};
    zpp::bits::in input(*file_bytes, zpp::bits::alloc_limit<max_header_size>());
    if (const zpp::bits::errc result = input(header); zpp::bits::failure(result)) {
        return Error::fail(Error::Code::CorruptData, "deserialize envelope header of \"" + path.string() + "\"", std::make_error_code(result.code));
    }
    if (auto valid = detail::validate_header(header); !valid) {
        return Error::propagate(std::move(valid), "decoding envelope header of \"" + path.string() + "\"");
    }
    return header;
}

namespace detail {

    Expected<void> validate_header(const Header& header)
    {
        if (header.magic != magic) {
            return Error::fail(Error::Code::CorruptData, "invalid envelope magic");
        }
        if (header.class_name.size() > max_class_name_size || header.hash.size() > max_hash_size) {
            return Error::fail(Error::Code::CorruptData, "envelope header exceeds its size limits");
        }
        if (!magic_enum::enum_contains(header.hash_algorithm)) {
            return Error::fail(Error::Code::Unsupported, "unsupported hash algorithm " + std::to_string(static_cast<unsigned>(header.hash_algorithm)));
        }
        if (!magic_enum::enum_contains(header.compression_algorithm)) {
            return Error::fail(
                Error::Code::Unsupported, "unsupported compression algorithm " + std::to_string(static_cast<unsigned>(header.compression_algorithm)));
        }
        return {};
    }

} // namespace detail

} // namespace io::envelope
