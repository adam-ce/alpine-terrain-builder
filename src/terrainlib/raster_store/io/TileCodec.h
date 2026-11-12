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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "raster/algorithm/copy.h"
#include "raster_store/AttributedRaster.h"
#include "raster_store/Tile.h"
#include "raster_store/io/ContainerCodec.h"
#include "raster_store/io/SubtileCoder.h"
#include "raster_store/io/definitions.h"
#include "raster_store/pixel.h"
#include "store/Codec.h"

namespace raster_store::io {

// Stores a tile as a container of encoded subtiles: its container codec reads and writes the tile
// file, its subtile coder encodes and decodes the data of the subtiles. Both also serve to assemble
// tiles from the encoded subtiles of other tiles of the same codec without decoding them.
template <typename PixelType>
class TileCodec final : public store::Codec<Tile<PixelType>> {
public:
    // The subtile coder must support the encoding, see SubtileCoder::supports.
    explicit TileCodec(const glm::uvec2 dimensions = glm::uvec2(default_tile_side),
        const DataEncoding encoding = DataEncoding::RawRaster,
        const ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompression)
        : m_container_codec(encoding, dimensions, compression_algorithm)
        , m_subtile_coder(encoding)
    {
    }

    // The codec of a snapshot's codec selector, which must support the pixel type.
    static Expected<std::unique_ptr<TileCodec>> from_name(const std::string_view name,
        const glm::uvec2 dimensions,
        const ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompression)
    {
        for (const auto encoding : encodings) {
            if (codec_names(encoding).selector != name) {
                continue;
            }
            if (!SubtileCoder<PixelType>::supports(encoding)) {
                const auto required = encoding == DataEncoding::FloatPredictor ? pixel::identifier<float>() : pixel::identifier<glm::u8vec3>();
                return Error::fail(Error::Code::Unsupported, "the " + std::string(name) + " raster codec requires " + required + " pixels");
            }
            return std::make_unique<TileCodec>(dimensions, encoding, compression_algorithm);
        }
        return Error::fail(Error::Code::Unsupported, "unsupported raster codec selector: " + std::string(name));
    }

    const ContainerCodec& container_codec() const { return m_container_codec; }
    const SubtileCoder<PixelType>& subtile_coder() const { return m_subtile_coder; }

    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override { return { m_container_codec.path(node_path) }; }

    Expected<Tile<PixelType>> read(
        const std::filesystem::path& node_path, const std::optional<store::NodeDataHashView> expected_hash = std::nullopt) const override
    {
        auto subtiles = m_container_codec.read(node_path, expected_hash);
        if (!subtiles) {
            return Error::propagate(std::move(subtiles));
        }
        Tile<PixelType> tile(m_container_codec.tile_size());
        const unsigned count = m_container_codec.subtile_count();
        for (unsigned y = 0; y < count; ++y) {
            for (unsigned x = 0; x < count; ++x) {
                // Each encoded subtile is released once it is decoded.
                auto subtile = m_subtile_coder.decode(std::move((*subtiles)[std::size_t { y } * count + x]));
                if (!subtile) {
                    return Error::propagate(std::move(subtile), "read " + name() + " tile");
                }
                const auto region = m_container_codec.subtile_region({ x, y });
                copy_region(*subtile, glm::uvec2(0), &tile, region.min, region.size());
            }
        }
        return tile;
    }

    // Reads only the subtiles that overlap the region and verifies their hashes. The expected hash is
    // compared with the container's recorded hash, which is not verified against the container payload.
    Expected<Tile<PixelType>> read_part(const std::filesystem::path& node_path,
        const typename Tile<PixelType>::Part& region,
        const std::optional<store::NodeDataHashView> expected_hash = std::nullopt) const override
    {
        if (auto valid = m_container_codec.validate_layout(); !valid) {
            return Error::propagate(std::move(valid));
        }
        if (glm::any(glm::greaterThanEqual(region.min, region.max)) || glm::any(glm::greaterThan(region.max, m_container_codec.tile_size()))) {
            return Error::fail(Error::Code::InvalidInput, "a partial " + name() + " read requires a non-empty region inside the tile");
        }
        auto container = m_container_codec.open(node_path, expected_hash);
        if (!container) {
            return Error::propagate(std::move(container), "read part of " + name() + " tile");
        }
        Tile<PixelType> part(region.size());
        const glm::uvec2 first = region.min / subtile_side;
        const glm::uvec2 last = (region.max - 1u) / subtile_side;
        for (unsigned y = first.y; y <= last.y; ++y) {
            for (unsigned x = first.x; x <= last.x; ++x) {
                auto encoded = container->read_subtile({ x, y });
                if (!encoded) {
                    return Error::propagate(std::move(encoded), "read part of " + name() + " tile");
                }
                auto subtile = m_subtile_coder.decode(std::move(*encoded));
                if (!subtile) {
                    return Error::propagate(std::move(subtile), "read part of " + name() + " tile");
                }
                const auto bounds = m_container_codec.subtile_region({ x, y });
                const glm::uvec2 overlap_min = glm::max(region.min, bounds.min);
                const glm::uvec2 overlap_max = glm::min(region.max, bounds.max);
                copy_region(*subtile, overlap_min - bounds.min, &part, overlap_min - region.min, overlap_max - overlap_min);
            }
        }
        return part;
    }

    Expected<store::NodeDataHash> write(const std::filesystem::path& node_path, const Tile<PixelType>& tile) const override
    {
        if (auto valid = m_container_codec.validate_layout(); !valid) {
            return Error::propagate(std::move(valid));
        }
        const glm::uvec2 dimensions = m_container_codec.tile_size();
        if (tile.data.size() != dimensions || tile.attribution.size() != dimensions) {
            return Error::fail(Error::Code::InvalidInput, "both tile rasters must match snapshot dimensions");
        }
        const unsigned count = m_container_codec.subtile_count();
        std::vector<EncodedSubtile> encoded;
        encoded.reserve(std::size_t { count } * count);
        for (unsigned y = 0; y < count; ++y) {
            for (unsigned x = 0; x < count; ++x) {
                const auto region = m_container_codec.subtile_region({ x, y });
                AttributedRaster<PixelType> subtile(region.size());
                copy_region(tile, region.min, &subtile, glm::uvec2(0), region.size());
                auto result = m_subtile_coder.encode(subtile);
                if (!result) {
                    return Error::propagate(std::move(result), "encode " + name() + " tile");
                }
                encoded.push_back(std::move(*result));
            }
        }
        return m_container_codec.write(node_path, encoded);
    }

    bool supports_hashing() const override { return true; }

private:
    std::string name() const { return codec_name(m_container_codec.encoding()); }

    // Copies a region of both rasters.
    static void copy_region(const AttributedRaster<PixelType>& source,
        const glm::uvec2 source_origin,
        AttributedRaster<PixelType>* destination,
        const glm::uvec2 destination_origin,
        const glm::uvec2 size)
    {
        const auto copy = [&](const auto& from, auto& to) {
            Error::asserting_unwrap(raster::algorithm::copy(Error::asserting_unwrap(raster::make_view(from, glm::i64vec2(source_origin), size)),
                Error::asserting_unwrap(raster::make_view(to, glm::i64vec2(destination_origin), size))));
        };
        copy(source.data, destination->data);
        copy(source.attribution, destination->attribution);
    }

    ContainerCodec m_container_codec;
    SubtileCoder<PixelType> m_subtile_coder;
};

} // namespace raster_store::io
