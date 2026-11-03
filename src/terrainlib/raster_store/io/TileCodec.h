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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <zpp_bits.h>

#include "io/byte.h"
#include "io/envelope.h"
#include "io/image.h"
#include "raster/algorithm/copy.h"
#include "raster_store/Tile.h"
#include "raster_store/io/float_predictor.h"
#include "raster_store/pixel.h"
#include "store/Codec.h"

namespace raster_store::io {

namespace tile_codec {

    // A tile is stored as a row-major grid of subtiles of this side, anchored at pixel 0; the last row and
    // column hold the remainder. Changing it requires a new TileContainer version.
    inline constexpr unsigned subtile_side = 256;

    namespace detail::v1 {

        struct RasterTile {
            std::uint32_t width;
            std::uint32_t height;
            // Native bytes for AMORT, JPEG XL for AMOIT, JPEG for AMOJT, Predictor 3 bytes for AMOFT.
            ::io::byte::vector data;
            ::io::byte::vector source_attribution;
        };

        // Subtile s is the envelope data[subtile_offsets[s], subtile_offsets[s + 1]). The offsets have one
        // entry per subtile and a final one, data.size().
        struct TileContainer {
            std::vector<std::uint64_t> subtile_offsets;
            ::io::byte::vector data;
        };

    } // namespace detail::v1

    // The file envelope, uncompressed, whose subtiles are envelopes of the codec's subtile schema below.
    using TileContainerSchema = ::io::envelope::PayloadSchema<"raster_store.TileContainer", ::io::envelope::Version<1, detail::v1::TileContainer>>;

    using TileSchema = ::io::envelope::PayloadSchema<"raster_store.Tile", ::io::envelope::Version<1, detail::v1::RasterTile>>;
    using ImageTileSchema = ::io::envelope::PayloadSchema<"raster_store.ImageTile", ::io::envelope::Version<1, detail::v1::RasterTile>>;
    using JpegTileSchema = ::io::envelope::PayloadSchema<"raster_store.JpegTile", ::io::envelope::Version<1, detail::v1::RasterTile>>;

    using FloatTileSchema = ::io::envelope::PayloadSchema<"raster_store.FloatTile", ::io::envelope::Version<1, detail::v1::RasterTile>>;

    // Storage of the data raster; attribution is always stored losslessly.
    enum class DataEncoding {
        Native, // AMORT: lossless native element bytes of any pixel type.
        JpegXl, // AMOIT: lossy JPEG XL image of RGB8 pixels.
        FloatPredictor, // AMOFT: lossless TIFF Predictor 3 bytes of Float32 pixels.
        Jpeg, // AMOJT: lossy JPEG image of RGB8 pixels.
    };

} // namespace tile_codec

template <typename PixelType, tile_codec::DataEncoding Encoding = tile_codec::DataEncoding::Native>
class TileCodec final : public store::Codec<Tile<PixelType>> {
    static_assert(sizeof(pixel::detail::Format<PixelType>) > 0);
    static constexpr bool is_jpeg = Encoding == tile_codec::DataEncoding::Jpeg;
    static constexpr bool is_image = is_jpeg || Encoding == tile_codec::DataEncoding::JpegXl;
    static_assert(!is_image || std::is_same_v<PixelType, glm::u8vec3>, "AMOIT and AMOJT store only RGB8 tiles");
    static constexpr bool is_float = Encoding == tile_codec::DataEncoding::FloatPredictor;
    static_assert(!is_float || (std::is_same_v<PixelType, float> && sizeof(float) == 4 && std::numeric_limits<float>::is_iec559),
        "AMOFT stores only IEEE 754 Float32 tiles");
    using Schema = std::conditional_t<is_jpeg,
        tile_codec::JpegTileSchema,
        std::conditional_t<is_image, tile_codec::ImageTileSchema, std::conditional_t<is_float, tile_codec::FloatTileSchema, tile_codec::TileSchema>>>;
    static constexpr std::string_view name = is_jpeg ? "AMOJT" : is_image ? "AMOIT" : is_float ? "AMOFT" : "AMORT";

public:
    explicit TileCodec(const glm::uvec2 dimensions = glm::uvec2(default_tile_side),
        const ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompression)
        : m_dimensions(dimensions)
        , m_compression_algorithm(compression_algorithm)
    {
    }

    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { this->add_extension(node_path, is_jpeg ? ".amojt" : is_image ? ".amoit" : is_float ? ".amoft" : ".amort") };
    }

    Expected<Tile<PixelType>> read(
        const std::filesystem::path& node_path, const std::optional<store::NodeDataHashView> expected_hash = std::nullopt) const override
    {
        if (auto valid = validate_layout(); !valid) {
            return Error::propagate(std::move(valid));
        }
        auto container = ::io::envelope::read<tile_codec::TileContainerSchema>(paths(node_path).front(), expected_hash);
        if (!container) {
            return Error::propagate(std::move(container), "read " + std::string(name) + " tile");
        }
        if (auto valid = validate_subtile_offsets(container->subtile_offsets, container->data.size()); !valid) {
            return Error::propagate(std::move(valid), "read " + std::string(name) + " tile");
        }
        auto tile = decode_region({ glm::uvec2(0), m_dimensions }, container->subtile_offsets, [&](const std::uint64_t offset, const std::uint64_t size) {
            return Expected<::io::byte::span>(::io::byte::span(container->data).subspan(offset, size));
        });
        if (!tile) {
            return Error::propagate(std::move(tile), "read " + std::string(name) + " tile");
        }
        return tile;
    }

    // Reads only the subtiles that overlap the region and verifies their hashes. The expected hash is
    // compared with the container's recorded hash, which is not verified against the container payload.
    Expected<Tile<PixelType>> read_part(const std::filesystem::path& node_path,
        const typename Tile<PixelType>::Part& region,
        const std::optional<store::NodeDataHashView> expected_hash = std::nullopt) const override
    {
        if (auto valid = validate_layout(); !valid) {
            return Error::propagate(std::move(valid));
        }
        if (glm::any(glm::greaterThanEqual(region.min, region.max)) || glm::any(glm::greaterThan(region.max, m_dimensions))) {
            return Error::fail(Error::Code::InvalidInput, "a partial " + std::string(name) + " read requires a non-empty region inside the tile");
        }
        auto container = open_container(paths(node_path).front(), expected_hash);
        if (!container) {
            return Error::propagate(std::move(container), "read part of " + std::string(name) + " tile");
        }
        auto part = decode_region(region, container->subtile_offsets, [&](const std::uint64_t offset, const std::uint64_t size) {
            return container->file.read(container->data_offset + offset, size);
        });
        if (!part) {
            return Error::propagate(std::move(part), "read part of " + std::string(name) + " tile");
        }
        return part;
    }

    Expected<store::NodeDataHash> write(const std::filesystem::path& node_path, const Tile<PixelType>& tile) const override
    {
        if (auto valid = validate_layout(); !valid) {
            return Error::propagate(std::move(valid));
        }
        if (tile.data.size() != m_dimensions || tile.attribution.size() != m_dimensions) {
            return Error::fail(Error::Code::InvalidInput, "both tile rasters must match snapshot dimensions");
        }
        tile_codec::detail::v1::TileContainer container { { 0 }, {} };
        for (unsigned y = 0; y < subtile_count(); ++y) {
            for (unsigned x = 0; x < subtile_count(); ++x) {
                const auto region = subtile_region({ x, y });
                Tile<PixelType> subtile(region.size());
                copy_region(tile, region.min, &subtile, glm::uvec2(0), region.size());
                auto encoded = encode_subtile(subtile);
                if (!encoded) {
                    return Error::propagate(std::move(encoded), "encode " + std::string(name) + " tile");
                }
                container.data.insert(container.data.end(), encoded->begin(), encoded->end());
                container.subtile_offsets.push_back(container.data.size());
            }
        }
        return ::io::envelope::write<tile_codec::TileContainerSchema>(
            container, paths(node_path).front(), ::io::byte::WriteMode::CreateNew, true, ::io::envelope::CompressionAlgorithm::None);
    }

    bool supports_hashing() const override { return true; }

private:
    // A tile container opened for partial reads, whose subtiles have not been read.
    struct OpenedContainer {
        ::io::byte::RandomAccessFile file;
        std::vector<std::uint64_t> subtile_offsets;
        // The file offset of the container data.
        std::uint64_t data_offset;
    };

    unsigned subtile_count() const { return m_dimensions.x / tile_codec::subtile_side + (m_dimensions.x % tile_codec::subtile_side != 0); }

    // The subtile offsets must fit a vector with a 32-bit size prefix.
    Expected<void> validate_layout() const
    {
        if (auto valid = validate_dimensions(m_dimensions); !valid) {
            return valid;
        }
        if (std::uint64_t { subtile_count() } * subtile_count() + 1 > std::numeric_limits<std::uint32_t>::max()) {
            return Error::fail(Error::Code::Unsupported, std::string(name) + " tile dimensions exceed the subtile count of a tile container");
        }
        return {};
    }

    radix::geometry::Aabb2ui subtile_region(const glm::uvec2 subtile) const
    {
        const glm::uvec2 min = subtile * tile_codec::subtile_side;
        return { min, glm::min(min + tile_codec::subtile_side, m_dimensions) };
    }

    // Copies a region of both rasters.
    static void copy_region(
        const Tile<PixelType>& source, const glm::uvec2 source_origin, Tile<PixelType>* destination, const glm::uvec2 destination_origin, const glm::uvec2 size)
    {
        const auto copy = [&](const auto& from, auto& to) {
            Error::asserting_unwrap(raster::algorithm::copy(Error::asserting_unwrap(raster::make_view(from, glm::i64vec2(source_origin), size)),
                Error::asserting_unwrap(raster::make_view(to, glm::i64vec2(destination_origin), size))));
        };
        copy(source.data, destination->data);
        copy(source.attribution, destination->attribution);
    }

    Expected<void> validate_subtile_offsets(const std::span<const std::uint64_t> offsets, const std::uint64_t data_size) const
    {
        if (offsets.size() != std::uint64_t { subtile_count() } * subtile_count() + 1 || offsets.front() != 0 || offsets.back() != data_size
            || !std::ranges::is_sorted(offsets)) {
            return Error::fail(Error::Code::CorruptData, std::string(name) + " subtile offsets do not match the tile dimensions and data");
        }
        return {};
    }

    // Reads the container header and subtile offsets, and compares the recorded hash with the expected one.
    Expected<OpenedContainer> open_container(const std::filesystem::path& path, const std::optional<store::NodeDataHashView> expected_hash) const
    {
        using ContainerSchema = tile_codec::TileContainerSchema;
        auto file = ::io::byte::RandomAccessFile::open(path);
        if (!file) {
            return Error::propagate(std::move(file));
        }
        // The payload starts with the offsets and the data size, both after a 32-bit length prefix.
        const std::uint64_t offset_count = std::uint64_t { subtile_count() } * subtile_count() + 1;
        const std::uint64_t prefix_size
            = ::io::envelope::max_prefix_size + sizeof(std::uint32_t) + offset_count * sizeof(std::uint64_t) + sizeof(std::uint32_t);
        auto prefix = file->read(0, std::min(prefix_size, file->size()));
        if (!prefix) {
            return Error::propagate(std::move(prefix));
        }
        auto location = ::io::envelope::locate_payload(*prefix);
        if (!location) {
            return Error::propagate(std::move(location), "read tile container header");
        }
        const auto& header = location->header;
        if (header.class_name != ContainerSchema::class_name) {
            return Error::fail(Error::Code::CorruptData,
                "unexpected envelope class \"" + header.class_name + "\", expected \"" + std::string(ContainerSchema::class_name) + "\"");
        }
        if (!ContainerSchema::supports_version_number(header.class_version)) {
            return Error::fail(Error::Code::Unsupported, "unsupported envelope class version " + std::to_string(header.class_version));
        }
        if (header.compression_algorithm != ::io::envelope::CompressionAlgorithm::None) {
            return Error::fail(Error::Code::Unsupported, "partial reads require an uncompressed tile container");
        }
        if (header.uncompressed_size != location->size) {
            return Error::fail(Error::Code::CorruptData, "tile container payload size does not match the envelope declaration");
        }
        if (expected_hash.has_value() && !std::ranges::equal(header.hash, *expected_hash)) {
            return Error::fail(Error::Code::CorruptData, "envelope payload hash does not match the expected hash");
        }
        if (location->offset + location->size != file->size()) {
            return Error::fail(Error::Code::CorruptData, "tile container payload does not fill its file");
        }

        // Check the offset count before allocating the offsets.
        const auto payload = ::io::byte::span(*prefix).subspan(location->offset);
        zpp::bits::in input(payload);
        std::uint32_t stored_offset_count = 0;
        if (zpp::bits::failure(input(stored_offset_count)) || stored_offset_count != offset_count) {
            return Error::fail(Error::Code::CorruptData, std::string(name) + " subtile offsets do not match the tile dimensions and data");
        }
        std::vector<std::uint64_t> subtile_offsets(stored_offset_count);
        std::uint32_t data_size = 0;
        if (zpp::bits::failure(input(zpp::bits::unsized(subtile_offsets), data_size)) || input.position() + data_size != location->size) {
            return Error::fail(Error::Code::CorruptData, "tile container data does not fill its payload");
        }
        if (auto valid = validate_subtile_offsets(subtile_offsets, data_size); !valid) {
            return Error::propagate(std::move(valid));
        }
        return OpenedContainer { std::move(*file), std::move(subtile_offsets), location->offset + input.position() };
    }

    Expected<::io::byte::vector> encode_subtile(const Tile<PixelType>& subtile) const
    {
        const glm::uvec2 size = subtile.data.size();
        const auto source_attribution = subtile.attribution.bytes();
        tile_codec::detail::v1::RasterTile encoded { size.x, size.y, {}, { source_attribution.begin(), source_attribution.end() } };
        if constexpr (is_image) {
            auto data = ::io::image::encode(subtile.data, is_jpeg ? ::io::image::Format::Jpeg : ::io::image::Format::JpegXl);
            if (!data) {
                return Error::propagate(std::move(data), "encode " + std::string(name) + " subtile data");
            }
            encoded.data = std::move(*data);
        } else {
            const auto data = subtile.data.bytes();
            encoded.data.assign(data.begin(), data.end());
            if constexpr (is_float) {
                float_predictor::encode(encoded.data, size.x);
            }
        }
        return ::io::envelope::serialize<Schema>(encoded, m_compression_algorithm);
    }

    // Decodes the subtiles that overlap the region into a tile of the region's size. read_envelope(offset, size)
    // returns the bytes of a subtile envelope at the given offset into the container data.
    template <typename ReadEnvelope>
    Expected<Tile<PixelType>> decode_region(
        const radix::geometry::Aabb2ui& region, const std::span<const std::uint64_t> offsets, const ReadEnvelope& read_envelope) const
    {
        Tile<PixelType> part(region.size());
        const glm::uvec2 first = region.min / tile_codec::subtile_side;
        const glm::uvec2 last = (region.max - 1u) / tile_codec::subtile_side;
        for (unsigned y = first.y; y <= last.y; ++y) {
            for (unsigned x = first.x; x <= last.x; ++x) {
                const auto index = std::size_t { y } * subtile_count() + x;
                auto bytes = read_envelope(offsets[index], offsets[index + 1] - offsets[index]);
                if (!bytes) {
                    return Error::propagate(std::move(bytes));
                }
                const auto bounds = subtile_region({ x, y });
                auto subtile = decode_subtile(*bytes, bounds.size());
                if (!subtile) {
                    return subtile;
                }
                const glm::uvec2 overlap_min = glm::max(region.min, bounds.min);
                const glm::uvec2 overlap_max = glm::min(region.max, bounds.max);
                copy_region(*subtile, overlap_min - bounds.min, &part, overlap_min - region.min, overlap_max - overlap_min);
            }
        }
        return part;
    }

    // Deserializes a subtile envelope, which verifies its hash, and decodes it.
    Expected<Tile<PixelType>> decode_subtile(const ::io::byte::span bytes, const glm::uvec2 expected_size) const
    {
        auto encoded = ::io::envelope::deserialize<Schema>(bytes);
        if (!encoded) {
            return Error::propagate(std::move(encoded), "read " + std::string(name) + " subtile");
        }
        if (glm::uvec2(encoded->width, encoded->height) != expected_size) {
            return Error::fail(Error::Code::CorruptData, std::string(name) + " subtile dimensions do not match snapshot metadata");
        }
        const std::uint64_t count = std::uint64_t { encoded->width } * encoded->height;
        if ((!is_image && (encoded->data.size() % sizeof(PixelType) != 0 || encoded->data.size() / sizeof(PixelType) != count))
            || encoded->source_attribution.size() % sizeof(std::uint16_t) != 0
            || encoded->source_attribution.size() / sizeof(std::uint16_t) != count) {
            return Error::fail(Error::Code::CorruptData, std::string(name) + " raster buffer byte counts do not match dimensions and payload type");
        }
        Tile<PixelType> tile(expected_size);
        if constexpr (is_image) {
            auto data = ::io::image::decode_rgb8(encoded->data, expected_size);
            if (!data) {
                return Error::propagate(std::move(data), "decode " + std::string(name) + " subtile data");
            }
            tile.data = std::move(*data);
        } else {
            if constexpr (is_float) {
                float_predictor::decode(encoded->data, expected_size.x);
            }
            std::memcpy(tile.data.bytes().data(), encoded->data.data(), encoded->data.size());
        }
        std::memcpy(tile.attribution.bytes().data(), encoded->source_attribution.data(), encoded->source_attribution.size());
        return tile;
    }

    glm::uvec2 m_dimensions;
    ::io::envelope::CompressionAlgorithm m_compression_algorithm;
};

namespace tile_codec {

    template <typename PixelType>
    Expected<std::unique_ptr<store::Codec<Tile<PixelType>>>> from_name(const std::string_view name,
        const glm::uvec2 dimensions,
        const ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompression)
    {
        if (name == "amort") {
            return std::make_unique<TileCodec<PixelType>>(dimensions, compression_algorithm);
        }
        if (name == "amoit" || name == "amojt") {
            if constexpr (std::is_same_v<PixelType, glm::u8vec3>) {
                if (name == "amojt") {
                    return std::make_unique<TileCodec<PixelType, DataEncoding::Jpeg>>(dimensions, compression_algorithm);
                }
                return std::make_unique<TileCodec<PixelType, DataEncoding::JpegXl>>(dimensions, compression_algorithm);
            } else {
                return Error::fail(
                    Error::Code::Unsupported, "the " + std::string(name) + " raster codec requires " + pixel::identifier<glm::u8vec3>() + " pixels");
            }
        }
        if (name == "amoft") {
            if constexpr (std::is_same_v<PixelType, float>) {
                return std::make_unique<TileCodec<PixelType, DataEncoding::FloatPredictor>>(dimensions, compression_algorithm);
            } else {
                return Error::fail(Error::Code::Unsupported, "the amoft raster codec requires " + pixel::identifier<float>() + " pixels");
            }
        }
        return Error::fail(Error::Code::Unsupported, "unsupported raster codec selector: " + std::string(name));
    }

} // namespace tile_codec

} // namespace raster_store::io
