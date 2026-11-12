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

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <libassert/assert.hpp>

#include "io/byte.h"
#include "io/envelope.h"

namespace raster_store::io {

// A tile is stored as a row-major grid of subtiles of this side, anchored at pixel 0; the last row and
// column hold the remainder. Changing it requires a new TileContainer version.
inline constexpr unsigned subtile_side = 256;

// Storage of the data raster; attribution is always stored losslessly.
enum class DataEncoding {
    RawRaster, // AMORT: lossless native element bytes of any pixel type.
    JpegXl, // AMOIT: lossy JPEG XL image of RGB8 pixels.
    FloatPredictor, // AMOFT: lossless TIFF Predictor 3 bytes of Float32 pixels.
    Jpeg, // AMOJT: lossy JPEG image of RGB8 pixels.
};

inline constexpr std::array encodings { DataEncoding::RawRaster, DataEncoding::JpegXl, DataEncoding::FloatPredictor, DataEncoding::Jpeg };

// The names of the tile codec of a data encoding.
struct CodecNames {
    // Selects the codec in snapshot metadata.
    std::string_view selector;
    // Names the codec in messages.
    std::string_view name;
    // The extension of the codec's tile files.
    std::string_view extension;
};

constexpr CodecNames codec_names(const DataEncoding encoding)
{
    switch (encoding) {
    case DataEncoding::RawRaster:
        return { "amort", "AMORT", ".amort" };
    case DataEncoding::JpegXl:
        return { "amoit", "AMOIT", ".amoit" };
    case DataEncoding::FloatPredictor:
        return { "amoft", "AMOFT", ".amoft" };
    case DataEncoding::Jpeg:
        return { "amojt", "AMOJT", ".amojt" };
    }
    UNREACHABLE();
}

inline std::string codec_name(const DataEncoding encoding) { return std::string(codec_names(encoding).name); }

namespace schema {

    namespace v1 {

        struct EncodedSubtile {
            std::uint32_t width;
            std::uint32_t height;
            // Raw raster bytes for AMORT, JPEG XL for AMOIT, JPEG for AMOJT, Predictor 3 bytes for AMOFT.
            ::io::byte::vector data;
            ::io::byte::vector source_attribution;

            bool operator==(const EncodedSubtile&) const = default;
        };

        // Precedes the container data, so that partial reads fetch the offsets without the subtiles.
        struct SubtileIndex {
            std::vector<std::uint64_t> subtile_offsets;
        };

        // Subtile s is the envelope data[subtile_offsets[s], subtile_offsets[s + 1]). The offsets have one
        // entry per subtile and a final one, data.size().
        struct TileContainer {
            SubtileIndex index;
            ::io::byte::vector data;
        };

    } // namespace v1

    // The envelope classes of stored subtiles, one per data encoding.
    using RawRasterSubtileSchema = ::io::envelope::PayloadSchema<"raster_store.Tile", ::io::envelope::Version<1, v1::EncodedSubtile>>;
    using JpegXlSubtileSchema = ::io::envelope::PayloadSchema<"raster_store.ImageTile", ::io::envelope::Version<1, v1::EncodedSubtile>>;
    using JpegSubtileSchema = ::io::envelope::PayloadSchema<"raster_store.JpegTile", ::io::envelope::Version<1, v1::EncodedSubtile>>;
    using FloatPredictorSubtileSchema = ::io::envelope::PayloadSchema<"raster_store.FloatTile", ::io::envelope::Version<1, v1::EncodedSubtile>>;

    // The file envelope, uncompressed, whose subtiles are envelopes of the subtile schema of the data encoding.
    using TileContainerSchema = ::io::envelope::PayloadSchema<"raster_store.TileContainer", ::io::envelope::Version<1, v1::TileContainer>>;

} // namespace schema

// A subtile as its tile file stores it: the data encoded, the attribution as native uint16 bytes.
using EncodedSubtile = schema::v1::EncodedSubtile;

} // namespace raster_store::io
