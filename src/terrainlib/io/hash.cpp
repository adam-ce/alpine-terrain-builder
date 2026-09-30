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

#include "io/hash.h"

#define XXH_INLINE_ALL
#include <xxhash.h>

#include <libassert/assert.hpp>

namespace io::hash {
namespace {

    std::vector<std::byte> xxh3_64(const std::span<const std::byte> bytes)
    {
        XXH64_canonical_t canonical;
        XXH64_canonicalFromHash(&canonical, XXH3_64bits(bytes.data(), bytes.size()));
        const auto* digest = reinterpret_cast<const std::byte*>(canonical.digest);
        return { digest, digest + sizeof(canonical.digest) };
    }

} // namespace

std::vector<std::byte> data(const std::span<const std::byte> bytes, const Algorithm algorithm)
{
    switch (algorithm) {
    case Algorithm::None:
        return {};
    case Algorithm::Xxh3_64:
        return xxh3_64(bytes);
    }
    PANIC("unsupported hash algorithm", static_cast<unsigned>(algorithm));
}

} // namespace io::hash
