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

#include "raster_store/io/ContainerCodec.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include <libassert/assert.hpp>
#include <zpp_bits.h>

#include "raster_store/Tile.h"

namespace raster_store::io {

namespace {

    Expected<::io::byte::vector> serialize_subtile(
        const EncodedSubtile& subtile, const DataEncoding encoding, const ::io::envelope::CompressionAlgorithm compression_algorithm)
    {
        switch (encoding) {
        case DataEncoding::RawRaster:
            return ::io::envelope::serialize<schema::RawRasterSubtileSchema>(subtile, compression_algorithm);
        case DataEncoding::JpegXl:
            return ::io::envelope::serialize<schema::JpegXlSubtileSchema>(subtile, compression_algorithm);
        case DataEncoding::FloatPredictor:
            return ::io::envelope::serialize<schema::FloatPredictorSubtileSchema>(subtile, compression_algorithm);
        case DataEncoding::Jpeg:
            return ::io::envelope::serialize<schema::JpegSubtileSchema>(subtile, compression_algorithm);
        }
        UNREACHABLE();
    }

    Expected<EncodedSubtile> deserialize_subtile(const ::io::byte::span envelope, const DataEncoding encoding)
    {
        switch (encoding) {
        case DataEncoding::RawRaster:
            return ::io::envelope::deserialize<schema::RawRasterSubtileSchema>(envelope);
        case DataEncoding::JpegXl:
            return ::io::envelope::deserialize<schema::JpegXlSubtileSchema>(envelope);
        case DataEncoding::FloatPredictor:
            return ::io::envelope::deserialize<schema::FloatPredictorSubtileSchema>(envelope);
        case DataEncoding::Jpeg:
            return ::io::envelope::deserialize<schema::JpegSubtileSchema>(envelope);
        }
        UNREACHABLE();
    }

} // namespace

ContainerCodec::ContainerCodec(const DataEncoding encoding, const glm::uvec2 tile_size, const ::io::envelope::CompressionAlgorithm compression_algorithm)
    : m_encoding(encoding)
    , m_tile_size(tile_size)
    , m_compression_algorithm(compression_algorithm)
{
}

std::filesystem::path ContainerCodec::path(const std::filesystem::path& node_path) const
{
    auto path = node_path;
    path += codec_names(m_encoding).extension;
    return path;
}

Expected<void> ContainerCodec::validate_layout() const
{
    if (auto valid = validate_dimensions(m_tile_size); !valid) {
        return valid;
    }
    // The subtile offsets must fit a vector with a 32-bit size prefix.
    if (std::uint64_t { subtile_count() } * subtile_count() + 1 > std::numeric_limits<std::uint32_t>::max()) {
        return Error::fail(Error::Code::Unsupported, codec_name(m_encoding) + " tile dimensions exceed the subtile count of a tile container");
    }
    return {};
}

unsigned ContainerCodec::subtile_count() const { return m_tile_size.x / subtile_side + (m_tile_size.x % subtile_side != 0); }

radix::geometry::Aabb2ui ContainerCodec::subtile_region(const glm::uvec2 position) const
{
    const glm::uvec2 min = position * subtile_side;
    return { min, glm::min(min + subtile_side, m_tile_size) };
}

Expected<store::NodeDataHash> ContainerCodec::write(const std::filesystem::path& node_path, const std::span<const EncodedSubtile> subtiles) const
{
    if (auto valid = validate_layout(); !valid) {
        return Error::propagate(std::move(valid));
    }
    const unsigned count = subtile_count();
    if (subtiles.size() != std::uint64_t { count } * count) {
        return Error::fail(Error::Code::InvalidInput, "a " + codec_name(m_encoding) + " tile requires one subtile per grid position");
    }
    schema::v1::TileContainer container { { { 0 } }, {} };
    container.index.subtile_offsets.reserve(subtiles.size() + 1);
    for (std::size_t index = 0; index < subtiles.size(); ++index) {
        const auto& subtile = subtiles[index];
        if (auto valid = validate_subtile(subtile, glm::uvec2(index % count, index / count), Error::Code::InvalidInput); !valid) {
            return Error::propagate(std::move(valid));
        }
        auto envelope = serialize_subtile(subtile, m_encoding, m_compression_algorithm);
        if (!envelope) {
            return Error::propagate(std::move(envelope), "write " + codec_name(m_encoding) + " subtile");
        }
        container.data.insert(container.data.end(), envelope->begin(), envelope->end());
        container.index.subtile_offsets.push_back(container.data.size());
    }
    return ::io::envelope::write<schema::TileContainerSchema>(
        container, path(node_path), ::io::byte::WriteMode::CreateNew, true, ::io::envelope::CompressionAlgorithm::None);
}

Expected<std::vector<EncodedSubtile>> ContainerCodec::read(
    const std::filesystem::path& node_path, const std::optional<store::NodeDataHashView> expected_hash) const
{
    if (auto valid = validate_layout(); !valid) {
        return Error::propagate(std::move(valid));
    }
    auto container = ::io::envelope::read<schema::TileContainerSchema>(path(node_path), expected_hash);
    if (!container) {
        return Error::propagate(std::move(container), "read " + codec_name(m_encoding) + " tile");
    }
    const auto& offsets = container->index.subtile_offsets;
    if (auto valid = validate_offsets(offsets, container->data.size()); !valid) {
        return Error::propagate(std::move(valid), "read " + codec_name(m_encoding) + " tile");
    }
    const unsigned count = subtile_count();
    std::vector<EncodedSubtile> subtiles;
    subtiles.reserve(offsets.size() - 1);
    for (std::size_t index = 0; index + 1 < offsets.size(); ++index) {
        auto subtile = deserialize(
            ::io::byte::span(container->data).subspan(offsets[index], offsets[index + 1] - offsets[index]), glm::uvec2(index % count, index / count));
        if (!subtile) {
            return Error::propagate(std::move(subtile));
        }
        subtiles.push_back(std::move(*subtile));
    }
    return subtiles;
}

Expected<ContainerCodec::Reader> ContainerCodec::open(const std::filesystem::path& node_path, const std::optional<store::NodeDataHashView> expected_hash) const
{
    using ContainerSchema = schema::TileContainerSchema;
    if (auto valid = validate_layout(); !valid) {
        return Error::propagate(std::move(valid));
    }
    auto file = ::io::byte::RandomAccessFile::open(path(node_path));
    if (!file) {
        return Error::propagate(std::move(file));
    }
    // The payload starts with the offsets and the data size, both after a 32-bit length prefix.
    const std::uint64_t offset_count = std::uint64_t { subtile_count() } * subtile_count() + 1;
    const std::uint64_t prefix_size = ::io::envelope::max_prefix_size + sizeof(std::uint32_t) + offset_count * sizeof(std::uint64_t) + sizeof(std::uint32_t);
    auto prefix = file->read(0, std::min(prefix_size, file->size()));
    if (!prefix) {
        return Error::propagate(std::move(prefix));
    }
    auto envelope_prefix = ::io::envelope::deserialize_prefix(*prefix);
    if (!envelope_prefix) {
        return Error::propagate(std::move(envelope_prefix), "read tile container header");
    }
    const auto& [header, location] = *envelope_prefix;
    if (header.class_name != ContainerSchema::class_name) {
        return Error::fail(
            Error::Code::CorruptData, "unexpected envelope class \"" + header.class_name + "\", expected \"" + std::string(ContainerSchema::class_name) + "\"");
    }
    if (!ContainerSchema::supports_version_number(header.class_version)) {
        return Error::fail(Error::Code::Unsupported, "unsupported envelope class version " + std::to_string(header.class_version));
    }
    if (header.compression_algorithm != ::io::envelope::CompressionAlgorithm::None) {
        return Error::fail(Error::Code::Unsupported, "partial reads require an uncompressed tile container");
    }
    if (header.uncompressed_size != location.size) {
        return Error::fail(Error::Code::CorruptData, "tile container payload size does not match the envelope declaration");
    }
    if (expected_hash.has_value() && !std::ranges::equal(header.hash, *expected_hash)) {
        return Error::fail(Error::Code::CorruptData, "envelope payload hash does not match the expected hash");
    }
    if (location.offset + location.size != file->size()) {
        return Error::fail(Error::Code::CorruptData, "tile container payload does not fill its file");
    }

    // Check the offset count before allocating the offsets.
    const auto payload = ::io::byte::span(*prefix).subspan(location.offset);
    zpp::bits::in input(payload);
    std::uint32_t stored_offset_count = 0;
    if (zpp::bits::failure(input(stored_offset_count)) || stored_offset_count != offset_count) {
        return Error::fail(Error::Code::CorruptData, codec_name(m_encoding) + " subtile offsets do not match the tile dimensions and data");
    }
    std::vector<std::uint64_t> subtile_offsets(stored_offset_count);
    std::uint32_t data_size = 0;
    if (zpp::bits::failure(input(zpp::bits::unsized(subtile_offsets), data_size)) || input.position() + data_size != location.size) {
        return Error::fail(Error::Code::CorruptData, "tile container data does not fill its payload");
    }
    if (auto valid = validate_offsets(subtile_offsets, data_size); !valid) {
        return Error::propagate(std::move(valid));
    }
    return Reader(*this, std::move(*file), std::move(subtile_offsets), location.offset + input.position());
}

Expected<void> ContainerCodec::validate_offsets(const std::span<const std::uint64_t> offsets, const std::uint64_t data_size) const
{
    if (offsets.size() != std::uint64_t { subtile_count() } * subtile_count() + 1 || offsets.front() != 0 || offsets.back() != data_size
        || !std::ranges::is_sorted(offsets)) {
        return Error::fail(Error::Code::CorruptData, codec_name(m_encoding) + " subtile offsets do not match the tile dimensions and data");
    }
    return {};
}

Expected<void> ContainerCodec::validate_subtile(const EncodedSubtile& subtile, const glm::uvec2 position, const Error::Code code) const
{
    const glm::uvec2 size = subtile_region(position).size();
    if (glm::uvec2(subtile.width, subtile.height) != size) {
        return Error::fail(code, codec_name(m_encoding) + " subtile dimensions do not match its region in the tile");
    }
    if (subtile.source_attribution.size() != std::uint64_t { size.x } * size.y * sizeof(std::uint16_t)) {
        return Error::fail(code, codec_name(m_encoding) + " subtile attribution byte count does not match its dimensions");
    }
    return {};
}

Expected<EncodedSubtile> ContainerCodec::deserialize(const ::io::byte::span envelope, const glm::uvec2 position) const
{
    auto subtile = deserialize_subtile(envelope, m_encoding);
    if (!subtile) {
        return Error::propagate(std::move(subtile), "read " + codec_name(m_encoding) + " subtile");
    }
    if (auto valid = validate_subtile(*subtile, position, Error::Code::CorruptData); !valid) {
        return Error::propagate(std::move(valid));
    }
    return subtile;
}

ContainerCodec::Reader::Reader(
    const ContainerCodec& codec, ::io::byte::RandomAccessFile file, std::vector<std::uint64_t> subtile_offsets, const std::uint64_t data_offset)
    : m_codec(codec)
    , m_file(std::move(file))
    , m_subtile_offsets(std::move(subtile_offsets))
    , m_data_offset(data_offset)
{
}

Expected<EncodedSubtile> ContainerCodec::Reader::read_subtile(const glm::uvec2 position) const
{
    const unsigned count = m_codec.subtile_count();
    if (glm::any(glm::greaterThanEqual(position, glm::uvec2(count)))) {
        return Error::fail(Error::Code::InvalidInput, "the subtile position lies outside the " + codec_name(m_codec.encoding()) + " tile");
    }
    const auto index = std::size_t { position.y } * count + position.x;
    auto envelope = m_file.read(m_data_offset + m_subtile_offsets[index], m_subtile_offsets[index + 1] - m_subtile_offsets[index]);
    if (!envelope) {
        return Error::propagate(std::move(envelope));
    }
    return m_codec.deserialize(*envelope, position);
}

} // namespace raster_store::io
