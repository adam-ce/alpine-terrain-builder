#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

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
    static Expected<TileWorker> open(const inputs::Record& record)
    {
        auto halo = nodata::halo(record.tile_side, record.nodata_search_radius, record.nodata_smoothing_kernel_size);
        if (!halo) {
            return Error::propagate(std::move(halo));
        }
        auto processor = nodata::Processor::create(record.nodata_search_radius, record.nodata_smoothing_kernel_size);
        if (!processor) {
            return Error::propagate(std::move(processor));
        }
        auto dataset = Dataset::open_raster(inputs::gdal_identifier(record.dataset));
        if (!dataset) {
            return Error::fail(Error::Code::InvalidInput, "open RF worker dataset", record.dataset);
        }
        auto transform = RasterTransform::create(*dataset->gdalDataset());
        if (!transform) {
            return Error::propagate(std::move(transform));
        }
        auto mask = Mask::open(inputs::gdal_identifier(record.mask));
        if (!mask) {
            return Error::propagate(std::move(mask));
        }
        return TileWorker(std::move(*dataset), std::move(*transform), std::move(*mask), record, *halo, std::move(*processor));
    }

    Expected<std::optional<raster_store::Tile<PixelType>>> prepare(const radix::tile::Id& key)
    {
        const auto window = nodata::window(key, m_record.tile_side, m_halo);
        const auto& bounds = window.bounds;
        auto samples = [&]() -> Expected<DatasetReader::Samples<PixelType>> {
            if constexpr (std::is_same_v<PixelType, float>) {
                return DatasetReader::read_scalar(*m_dataset.gdalDataset(), m_transform, bounds, window.size, m_record.bands[0]);
            } else {
                return DatasetReader::read_colour(
                    *m_dataset.gdalDataset(), m_transform, bounds, window.size, { m_record.bands[0], m_record.bands[1], m_record.bands[2] });
            }
        }();
        if (!samples) {
            return Error::propagate(std::move(samples), "read RF tile " + to_string(key));
        }
        const double spacing = bounds.width() / window.size.x;
        auto selected_validity = samples->valid;
        std::vector<glm::dvec2> centres(window.size.x);
        for (unsigned row = 0; row < window.size.y; ++row) {
            for (unsigned column = 0; column < window.size.x; ++column) {
                double x = bounds.min.x + (column + 0.5) * spacing;
                const double half = RasterTransform::world_half_extent;
                x -= 2 * half * std::floor((x + half) / (2 * half));
                centres[column] = { x, bounds.max.y - (row + 0.5) * spacing };
            }
            auto validity = std::span(selected_validity.buffer()).subspan(std::size_t(row) * window.size.x, window.size.x);
            if (auto selected = m_mask.select(centres, validity); !selected) {
                return Error::propagate(std::move(selected), "mask RF tile " + to_string(key));
            }
        }
        if (std::ranges::none_of(selected_validity.buffer(), [](auto value) { return value != 0; })) {
            return std::nullopt;
        }
        raster_store::Tile<PixelType> tile(m_record.tile_side);
        auto completed = m_processor.process(*samples, window.interior_offset, m_record.nodata_default_value, tile.data);
        if (!completed) {
            return Error::propagate(std::move(completed), "complete RF tile " + to_string(key));
        }
        const auto selected = *raster::make_view(selected_validity, window.interior_offset, glm::uvec2(m_record.tile_side));
        auto attributed = raster::algorithm::transform(
            selected, [&](std::uint8_t valid) -> std::uint16_t { return valid ? std::uint16_t(m_record.attribution_index) : 0; }, tile.source_attribution);
        if (!attributed) {
            return Error::propagate(std::move(attributed));
        }
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
