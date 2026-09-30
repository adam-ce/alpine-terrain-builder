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

#include "DatasetReader.h"
#include "raster/ClampedView.h"
#include "raster/algorithm.h"
#include <array>
#include <libassert/assert.hpp>
#include <limits>

namespace rf_builder::gdal::nodata {

// Validate halo/dimension arithmetic before opening inputs or planning tiles.
Expected<unsigned> halo(unsigned tile_side, unsigned search_radius, unsigned kernel_size);

struct Window {
    RasterTransform::Bounds bounds;
    glm::uvec2 size;
    glm::uvec2 interior_offset;
};
Window window(const radix::tile::Id& key, unsigned tile_side, unsigned halo_width);

class Processor {
public:
    static Expected<Processor> create(unsigned search_radius, unsigned kernel_size);

    // Throws Error::Exception when GDAL cannot fill the NoData pixels. The
    // samples must cover the square output at offset.
    template <typename PixelType>
    radix::Raster<PixelType> process(const DatasetReader::Samples<PixelType>& samples, glm::uvec2 offset, unsigned side, const std::array<float, 3>& fallback)
    {
        radix::Raster<PixelType> result(side);
        process(samples, offset, fallback, result);
        return result;
    }

    template <typename PixelType>
    void process(const DatasetReader::Samples<PixelType>& samples, glm::uvec2 offset, const std::array<float, 3>& fallback, radix::Raster<PixelType>& result)
    {
        const unsigned side = result.width();
        ASSERT(side > 0 && result.height() == side && samples.data.size() == samples.valid.size());
        const auto original = Error::asserting_unwrap(raster::make_view(samples.data, offset, glm::uvec2(side)));
        const auto valid = Error::asserting_unwrap(raster::make_view(samples.valid, offset, glm::uvec2(side)));
        if (std::ranges::all_of(samples.valid.buffer(), [](auto value) { return value != 0; })) {
            Error::asserting_unwrap(raster::algorithm::copy(original, result));
            return;
        }
        resize(m_work, samples.data.size());
        constexpr unsigned channels = std::is_same_v<PixelType, float> ? 1 : 3;
        for (unsigned channel = 0; channel < channels; ++channel) {
            Error::asserting_unwrap(raster::algorithm::zip_transform(
                samples.data,
                samples.valid,
                [&](const PixelType& value, std::uint8_t source_valid) -> float {
                    if (!source_valid) {
                        return fallback[channel];
                    }
                    if constexpr (std::is_same_v<PixelType, float>) {
                        return value;
                    } else {
                        return value[channel];
                    }
                },
                m_work));
            const auto completed = complete(samples.valid, offset, side);
            const auto output = raster::make_view(result);
            Error::asserting_unwrap(raster::algorithm::zip_transform(
                std::tuple { original, valid, completed, output },
                [&](const PixelType& value, std::uint8_t source_valid, float replacement, PixelType previous) -> PixelType {
                    if (source_valid) {
                        return value;
                    }
                    if constexpr (std::is_same_v<PixelType, float>) {
                        return replacement;
                    } else {
                        previous[channel] = std::get<1>(raster::algorithm::linear_conversion<std::uint8_t>())(replacement);
                        return previous;
                    }
                },
                result));
        }
    }

private:
    Processor(unsigned search_radius, std::vector<double> weights)
        : m_search_radius(search_radius)
        , m_weights(std::move(weights))
    {
    }
    static void resize(radix::Raster<float>& raster, glm::uvec2 size);
    raster::View<const float> complete(const radix::Raster<std::uint8_t>& valid, glm::uvec2 offset, unsigned side);

    unsigned m_search_radius;
    std::vector<double> m_weights;
    radix::Raster<float> m_work;
    radix::Raster<float> m_horizontal;
    radix::Raster<float> m_smoothed;
};
} // namespace rf_builder::gdal::nodata
