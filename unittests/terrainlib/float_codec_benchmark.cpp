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

#include "../temporary_directory.h"
#include "raster_store/io/TileCodec.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <span>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fmt/format.h>

// Explicit opt-in: AMOFT_BENCHMARK_TILE=/path/to/dsm.amort
// unittests_terrainlib '[amoft-benchmark]' --benchmark-samples 5
TEST_CASE("AMORT versus AMOFT whole-tile envelopes on identical Float32 RF data", "[.][amoft-benchmark]")
{
    namespace codecs = raster_store::io::tile_codec;
    using io::envelope::CompressionAlgorithm;
    const char* input = std::getenv("AMOFT_BENCHMARK_TILE");
    REQUIRE(input != nullptr);
    // The tile side follows from the subtile count and the width of the last subtile.
    const auto container = io::envelope::read<codecs::TileContainerSchema>(input);
    REQUIRE(container);
    const auto count = static_cast<unsigned>(std::lround(std::sqrt(double(container->subtile_offsets.size() - 1))));
    const auto last = io::envelope::deserialize<codecs::TileSchema>(std::span(container->data).subspan(container->subtile_offsets[count * count - 1]));
    REQUIRE(last);
    const unsigned side = (count - 1) * codecs::subtile_side + last->width;
    const auto stored = raster_store::io::TileCodec<float>(glm::uvec2(side)).read(std::filesystem::path(input).replace_extension());
    REQUIRE(stored);
    const codecs::detail::v1::RasterTile source {
        side, side, { stored->data.bytes().begin(), stored->data.bytes().end() }, { stored->attribution.bytes().begin(), stored->attribution.bytes().end() }
    };
    const auto predicted = GENERATE(false, true);
    const auto compression = GENERATE(CompressionAlgorithm::ZstdDefaultCompression, CompressionAlgorithm::ZstdBestCompression);
    const auto label = fmt::format(
        "{} / {} whole-tile envelope", predicted ? "AMOFT" : "AMORT", compression == CompressionAlgorithm::ZstdDefaultCompression ? "zstd" : "zstd-best");
    CAPTURE(input, label);

    // Match the codec's per-subtile copies, prediction, serialization, hashing,
    // compression, allocation, and raster reconstruction, but with the whole tile
    // as one subtile and excluding the container and filesystem operations.
    const auto encode = [&] {
        auto payload = source;
        if (predicted) {
            raster_store::io::float_predictor::encode(payload.data, payload.width);
            return Error::throwing_unwrap(io::envelope::serialize<codecs::FloatTileSchema>(payload, compression));
        }
        return Error::throwing_unwrap(io::envelope::serialize<codecs::TileSchema>(payload, compression));
    };
    const auto encoded = encode();
    const auto decode = [&] {
        auto payload = Error::throwing_unwrap(
            predicted ? io::envelope::deserialize<codecs::FloatTileSchema>(encoded) : io::envelope::deserialize<codecs::TileSchema>(encoded));
        raster_store::Tile<float> tile(payload.width);
        if (predicted) {
            raster_store::io::float_predictor::decode(payload.data, payload.width);
        }
        std::memcpy(tile.data.bytes().data(), payload.data.data(), payload.data.size());
        std::memcpy(tile.attribution.bytes().data(), payload.source_attribution.data(), payload.source_attribution.size());
        return tile;
    };
    const auto decoded = decode();
    REQUIRE(std::ranges::equal(decoded.data.bytes(), source.data));
    REQUIRE(std::ranges::equal(decoded.attribution.bytes(), source.source_attribution));

    // The codec stores 256-pixel subtiles, so its files differ from this whole-tile envelope.
    test::TemporaryDirectory directory;
    const auto codec = codecs::from_name<float>(predicted ? "amoft" : "amort", glm::uvec2(source.width), compression);
    REQUIRE(codec);
    const auto path = directory.path() / "tile";
    const auto hash = (*codec)->write(path, decoded);
    REQUIRE(hash);
    const auto roundtrip = (*codec)->read(path, *hash);
    REQUIRE(roundtrip);
    REQUIRE(std::ranges::equal(roundtrip->data.bytes(), source.data));
    REQUIRE(std::ranges::equal(roundtrip->attribution.bytes(), source.source_attribution));
    fmt::print("\n{}: {}x{}, {} bytes, {} bytes as codec file, bit-exact data and attribution\n",
        label,
        source.width,
        source.height,
        encoded.size(),
        std::filesystem::file_size((*codec)->paths(path).front()));
    BENCHMARK(label + " encode") { return encode(); };
    BENCHMARK(label + " decode") { return decode(); };
}
