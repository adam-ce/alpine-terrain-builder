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
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "Error.h"
#include "io/byte.h"
#include "io/compression.h"
#include "io/envelope.h"
#include "raster_store/io/definitions.h"
#include "store/Codec.h"

namespace raster_store::io {

// Reads and writes the tile file of a tile codec: a container of the tile's encoded subtiles, each in
// its own envelope. It knows the subtile grid and checks the subtiles' dimensions against it, but
// does not interpret their data.
class ContainerCodec {
public:
    class Reader;

    ContainerCodec(DataEncoding encoding,
        glm::uvec2 tile_size,
        ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompression);

    DataEncoding encoding() const { return m_encoding; }
    glm::uvec2 tile_size() const { return m_tile_size; }

    // The tile file of a node.
    std::filesystem::path path(const std::filesystem::path& node_path) const;

    // The tile must be square and the offsets of its subtiles must fit a container. Reads and writes
    // check it; the subtile grid below requires it.
    Expected<void> validate_layout() const;
    // The number of subtiles per axis.
    unsigned subtile_count() const;
    // The region of the subtile at a grid position in stored pixels.
    radix::geometry::Aabb2ui subtile_region(glm::uvec2 position) const;

    // Writes a new tile file from its subtiles in row-major order and returns its hash. Each subtile
    // must have the dimensions of its region.
    Expected<store::NodeDataHash> write(const std::filesystem::path& node_path, std::span<const EncodedSubtile> subtiles) const;

    // Reads all subtiles in row-major order. Verifies the hashes of the container and of each subtile,
    // and compares the container's with the expected hash.
    Expected<std::vector<EncodedSubtile>> read(
        const std::filesystem::path& node_path, std::optional<store::NodeDataHashView> expected_hash = std::nullopt) const;

    // Reads the container header and the subtile offsets for partial reads. The expected hash is
    // compared with the container's recorded hash, which is not verified against the container payload.
    Expected<Reader> open(const std::filesystem::path& node_path, std::optional<store::NodeDataHashView> expected_hash = std::nullopt) const;

private:
    Expected<void> validate_offsets(std::span<const std::uint64_t> offsets, std::uint64_t data_size) const;
    // The subtile must have the dimensions of its region and one attribution value per pixel.
    Expected<void> validate_subtile(const EncodedSubtile& subtile, glm::uvec2 position, Error::Code code) const;
    // Deserializes the envelope of the subtile at a grid position, which verifies its hash.
    Expected<EncodedSubtile> deserialize(::io::byte::span envelope, glm::uvec2 position) const;

    DataEncoding m_encoding;
    glm::uvec2 m_tile_size;
    ::io::envelope::CompressionAlgorithm m_compression_algorithm;
};

// A tile file opened for partial reads, whose subtiles have not been read.
class ContainerCodec::Reader {
public:
    // ContainerCodec::open checks the offsets against the codec's subtile grid.
    Reader(const ContainerCodec& codec, ::io::byte::RandomAccessFile file, std::vector<std::uint64_t> subtile_offsets, std::uint64_t data_offset);

    // Reads the subtile at a grid position and verifies its hash.
    Expected<EncodedSubtile> read_subtile(glm::uvec2 position) const;

private:
    ContainerCodec m_codec;
    ::io::byte::RandomAccessFile m_file;
    std::vector<std::uint64_t> m_subtile_offsets;
    // The file offset of the container data.
    std::uint64_t m_data_offset;
};

} // namespace raster_store::io
