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
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#include "Error.h"
#include "io/image.h"
#include "raster_store/AttributedRaster.h"
#include "raster_store/io/definitions.h"
#include "raster_store/io/float_predictor.h"
#include "raster_store/pixel.h"

namespace raster_store::io {

// Encodes and decodes the data of subtiles. It accesses neither files nor envelopes.
template <typename PixelType>
class SubtileCoder {
    static_assert(sizeof(pixel::detail::Format<PixelType>) > 0);

public:
    // AMOIT and AMOJT store only RGB8 subtiles, AMOFT only IEEE 754 Float32 ones.
    static constexpr bool supports(const DataEncoding encoding)
    {
        switch (encoding) {
        case DataEncoding::RawRaster:
            return true;
        case DataEncoding::JpegXl:
        case DataEncoding::Jpeg:
            return std::is_same_v<PixelType, glm::u8vec3>;
        case DataEncoding::FloatPredictor:
            return std::is_same_v<PixelType, float> && sizeof(float) == 4 && std::numeric_limits<float>::is_iec559;
        }
        return false;
    }

    explicit SubtileCoder(const DataEncoding encoding)
        : m_encoding(encoding)
    {
    }

    DataEncoding encoding() const { return m_encoding; }

    Expected<EncodedSubtile> encode(const AttributedRaster<PixelType>& subtile) const
    {
        if (auto valid = validate_encoding(); !valid) {
            return Error::propagate(std::move(valid));
        }
        const glm::uvec2 size = subtile.data.size();
        if (size.x == 0 || size.y == 0) {
            return Error::fail(Error::Code::InvalidInput, "subtile dimensions must be positive");
        }
        if (subtile.attribution.size() != size) {
            return Error::fail(Error::Code::InvalidInput, "both subtile rasters must have the same size");
        }
        const auto source_attribution = subtile.attribution.bytes();
        EncodedSubtile encoded { size.x, size.y, {}, { source_attribution.begin(), source_attribution.end() } };
        if constexpr (std::is_same_v<PixelType, glm::u8vec3>) {
            if (is_image()) {
                auto data = ::io::image::encode(subtile.data, is_jpeg() ? ::io::image::Format::Jpeg : ::io::image::Format::JpegXl);
                if (!data) {
                    return Error::propagate(std::move(data), "encode " + codec_name(m_encoding) + " subtile data");
                }
                encoded.data = std::move(*data);
                return encoded;
            }
        }
        const auto data = subtile.data.bytes();
        encoded.data.assign(data.begin(), data.end());
        if (is_float()) {
            float_predictor::encode(encoded.data, size.x);
        }
        return encoded;
    }

    // The data must decode to the subtile's width and height, which the attribution must match too.
    Expected<AttributedRaster<PixelType>> decode(EncodedSubtile subtile) const
    {
        if (auto valid = validate_encoding(); !valid) {
            return Error::propagate(std::move(valid));
        }
        const glm::uvec2 size(subtile.width, subtile.height);
        if (size.x == 0 || size.y == 0) {
            return Error::fail(Error::Code::CorruptData, "subtile dimensions must be positive");
        }
        const std::uint64_t count = std::uint64_t { size.x } * size.y;
        const auto holds = [count](const ::io::byte::vector& bytes, const std::size_t element_size) {
            return bytes.size() % element_size == 0 && bytes.size() / element_size == count;
        };
        if (!holds(subtile.source_attribution, sizeof(std::uint16_t)) || (!is_image() && !holds(subtile.data, sizeof(PixelType)))) {
            return Error::fail(Error::Code::CorruptData, codec_name(m_encoding) + " subtile byte counts do not match its dimensions and payload type");
        }
        AttributedRaster<PixelType> raster(size);
        if constexpr (std::is_same_v<PixelType, glm::u8vec3>) {
            if (is_image()) {
                auto data = ::io::image::decode_rgb8(subtile.data, size);
                if (!data) {
                    return Error::propagate(std::move(data), "decode " + codec_name(m_encoding) + " subtile data");
                }
                raster.data = std::move(*data);
            }
        }
        if (!is_image()) {
            if (is_float()) {
                float_predictor::decode(subtile.data, size.x);
            }
            std::ranges::copy(subtile.data, raster.data.bytes().begin());
        }
        std::ranges::copy(subtile.source_attribution, raster.attribution.bytes().begin());
        return raster;
    }

private:
    bool is_jpeg() const { return m_encoding == DataEncoding::Jpeg; }
    bool is_image() const { return is_jpeg() || m_encoding == DataEncoding::JpegXl; }
    bool is_float() const { return m_encoding == DataEncoding::FloatPredictor; }

    Expected<void> validate_encoding() const
    {
        if (!supports(m_encoding)) {
            return Error::fail(
                Error::Code::Unsupported, "the " + codec_name(m_encoding) + " raster codec does not store " + pixel::identifier<PixelType>() + " pixels");
        }
        return {};
    }

    DataEncoding m_encoding;
};

} // namespace raster_store::io
