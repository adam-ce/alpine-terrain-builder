/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
