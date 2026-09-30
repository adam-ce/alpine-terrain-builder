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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

#include "Dataset.h"
#include "DatasetReader.h"
#include "Mask.h"
#include "inputs.h"
#include "nodata.h"
#include "raster_store/Tile.h"

namespace rf_builder::gdal {

// One instance belongs to one worker for its entire lifetime. In particular,
// const query methods on GDAL/CGAL objects do not make them safe to share.
template <typename PixelType>
class TileWorker {
public:
    // Throws Error::Exception when the inputs cannot be opened. The record's
    // NoData settings must already be validated.
    static TileWorker open(const inputs::Record& record)
    {
        const auto halo = Error::asserting_unwrap(nodata::halo(record.tile_side, record.nodata_search_radius, record.nodata_smoothing_kernel_size));
        auto processor = Error::asserting_unwrap(nodata::Processor::create(record.nodata_search_radius, record.nodata_smoothing_kernel_size));
        auto dataset = Dataset::open_raster(inputs::gdal_identifier(record.dataset));
        if (!dataset) {
            Error::raise(Error::Code::InvalidInput, "open RF worker dataset", record.dataset);
        }
        auto transform = Error::throwing_unwrap(RasterTransform::create(*dataset->gdalDataset()));
        auto mask = Error::throwing_unwrap(Mask::open(inputs::gdal_identifier(record.mask)));
        return TileWorker(std::move(*dataset), std::move(transform), std::move(mask), record, halo, std::move(processor));
    }

    // Returns no tile when the mask excludes every sample; throws Error::Exception on failure.
    std::optional<raster_store::Tile<PixelType>> prepare(const radix::tile::Id& key)
    {
        const auto window = nodata::window(key, m_record.tile_side, m_halo);
        const auto& bounds = window.bounds;
        auto samples = Error::throwing_unwrap(
            [&]() -> Expected<DatasetReader::Samples<PixelType>> {
                if constexpr (std::is_same_v<PixelType, float>) {
                    return DatasetReader::read_scalar(*m_dataset.gdalDataset(), m_transform, bounds, window.size, m_record.bands[0]);
                } else {
                    return DatasetReader::read_colour(
                        *m_dataset.gdalDataset(), m_transform, bounds, window.size, { m_record.bands[0], m_record.bands[1], m_record.bands[2] });
                }
            }(),
            "read RF tile " + to_string(key));
        const double spacing = bounds.width() / window.size.x;
        auto selected_validity = samples.valid;
        std::vector<glm::dvec2> centres(window.size.x);
        for (unsigned row = 0; row < window.size.y; ++row) {
            for (unsigned column = 0; column < window.size.x; ++column) {
                double x = bounds.min.x + (column + 0.5) * spacing;
                const double half = RasterTransform::world_half_extent;
                x -= 2 * half * std::floor((x + half) / (2 * half));
                centres[column] = { x, bounds.max.y - (row + 0.5) * spacing };
            }
            auto validity = std::span(selected_validity.buffer()).subspan(std::size_t(row) * window.size.x, window.size.x);
            Error::throwing_unwrap(m_mask.select(centres, validity), "mask RF tile " + to_string(key));
        }
        if (std::ranges::none_of(selected_validity.buffer(), [](auto value) { return value != 0; })) {
            return std::nullopt;
        }
        raster_store::Tile<PixelType> tile(m_record.tile_side);
        m_processor.process(samples, window.interior_offset, m_record.nodata_default_value, tile.data);
        const auto selected = Error::asserting_unwrap(raster::make_view(selected_validity, window.interior_offset, glm::uvec2(m_record.tile_side)));
        Error::asserting_unwrap(raster::algorithm::transform(
            selected, [&](std::uint8_t valid) -> std::uint16_t { return valid ? std::uint16_t(m_record.attribution_index) : 0; }, tile.source_attribution));
        return std::optional(std::move(tile));
    }

private:
    TileWorker(Dataset dataset, RasterTransform transform, Mask mask, inputs::Record record, unsigned halo, nodata::Processor processor)
        : m_dataset(std::move(dataset))
        , m_transform(std::move(transform))
        , m_mask(std::move(mask))
        , m_record(std::move(record))
        , m_halo(halo)
        , m_processor(std::move(processor))
    {
    }
    Dataset m_dataset;
    RasterTransform m_transform;
    Mask m_mask;
    inputs::Record m_record;
    unsigned m_halo;
    nodata::Processor m_processor;
};
} // namespace rf_builder::gdal
