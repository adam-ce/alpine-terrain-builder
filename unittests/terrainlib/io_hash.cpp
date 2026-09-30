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

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace {

// XSUM_fillTestBuffer from xxHash's cli/xsum_sanity_check.c, which generates the input of its test vectors.
std::vector<std::byte> sanity_buffer(const std::size_t size)
{
    std::vector<std::byte> buffer(size);
    std::uint64_t generator = 2654435761U;
    for (auto& byte : buffer) {
        byte = std::byte(generator >> 56);
        generator *= 11400714785074694797ULL;
    }
    return buffer;
}

std::vector<std::byte> big_endian(const std::uint64_t value)
{
    std::vector<std::byte> bytes(8);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = std::byte(value >> (56 - 8 * index));
    }
    return bytes;
}

} // namespace

TEST_CASE("XXH3-64 matches the xxHash sanity test vectors in canonical byte order", "[io][hash]")
{
    struct Vector {
        std::size_t size;
        std::uint64_t hash;
    };
    // Seed 0 entries of XSUM_XXH3_testdata in xxHash v0.8.3, covering every input size class.
    constexpr std::array vectors {
        Vector { 0, 0x2D06800538D394C2ULL },
        Vector { 1, 0xC44BDFF4074EECDBULL },
        Vector { 6, 0x27B56A84CD2D7325ULL },
        Vector { 12, 0xA713DAF0DFBB77E7ULL },
        Vector { 24, 0xA3FE70BF9D3510EBULL },
        Vector { 48, 0x397DA259ECBA1F11ULL },
        Vector { 80, 0xBCDEFBBB2C47C90AULL },
        Vector { 195, 0xCD94217EE362EC3AULL },
        Vector { 403, 0xCDEB804D65C6DEA4ULL },
        Vector { 512, 0x617E49599013CB6BULL },
        Vector { 2048, 0xDD59E2C3A5F038E0ULL },
        Vector { 2099, 0xC6B9D9B3FC9AC765ULL },
        Vector { 2240, 0x6E73A90539CF2948ULL },
        Vector { 2367, 0xCB37AEB9E5D361EDULL },
    };
    const auto buffer = sanity_buffer(vectors.back().size);
    for (const auto& vector : vectors) {
        CAPTURE(vector.size);
        CHECK(io::hash::data(std::span(buffer).first(vector.size), io::hash::Algorithm::Xxh3_64) == big_endian(vector.hash));
    }
}

TEST_CASE("the None hash algorithm produces no bytes", "[io][hash]") { CHECK(io::hash::data(sanity_buffer(16), io::hash::Algorithm::None).empty()); }

TEST_CASE("XXH3-64 throughput over 1 GiB", "[!benchmark][io][hash]")
{
    const auto buffer = sanity_buffer(std::size_t { 1 } << 30);
    BENCHMARK("XXH3-64, 1 GiB") { return io::hash::data(buffer, io::hash::Algorithm::Xxh3_64); };
}
