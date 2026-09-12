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
#include "raster_store/Tile.h"

namespace rf_builder::gdal {

// One instance belongs to one worker for its entire lifetime. In particular,
// const query methods on GDAL/CGAL objects do not make them safe to share.
template <typename PixelType>
class TileWorker {
public:
    static Expected<TileWorker> open(const inputs::Record& record)
    {
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
        return TileWorker(std::move(*dataset), std::move(*transform), std::move(*mask), record);
    }

    Expected<std::optional<raster_store::Tile<PixelType>>> prepare(const radix::tile::Id& key)
    {
        const auto bounds = RasterTransform::tile_bounds(key);
        auto samples = [&]() -> Expected<DatasetReader::Samples<PixelType>> {
            if constexpr (std::is_same_v<PixelType, float>) {
                return DatasetReader::read_scalar(*m_dataset.gdalDataset(), m_transform, bounds, m_record.tile_side, m_record.bands[0]);
            } else {
                return DatasetReader::read_colour(
                    *m_dataset.gdalDataset(), m_transform, bounds, m_record.tile_side, { m_record.bands[0], m_record.bands[1], m_record.bands[2] });
            }
        }();
        if (!samples) {
            return Error::propagate(std::move(samples), "read RF tile " + to_string(key));
        }
        const double spacing = bounds.width() / m_record.tile_side;
        std::vector<glm::dvec2> centres(m_record.tile_side);
        for (unsigned row = 0; row < m_record.tile_side; ++row) {
            for (unsigned column = 0; column < m_record.tile_side; ++column) {
                centres[column] = { bounds.min.x + (column + 0.5) * spacing, bounds.max.y - (row + 0.5) * spacing };
            }
            auto validity = std::span(samples->valid.buffer()).subspan(std::size_t(row) * m_record.tile_side, m_record.tile_side);
            if (auto selected = m_mask.select(centres, validity); !selected) {
                return Error::propagate(std::move(selected), "mask RF tile " + to_string(key));
            }
        }
        if (std::ranges::none_of(samples->valid.buffer(), [](auto value) { return value != 0; })) {
            return std::nullopt;
        }
        raster_store::Tile<PixelType> tile(m_record.tile_side);
        tile.data = std::move(samples->data);
        for (std::size_t i = 0; i < samples->valid.buffer().size(); ++i) {
            if (samples->valid.buffer()[i]) {
                tile.source_attribution.buffer()[i] = std::uint16_t(m_record.attribution_index);
            }
        }
        return std::optional(std::move(tile));
    }

private:
    TileWorker(Dataset dataset, RasterTransform transform, Mask mask, inputs::Record record)
        : m_dataset(std::move(dataset))
        , m_transform(std::move(transform))
        , m_mask(std::move(mask))
        , m_record(std::move(record))
    {
    }
    Dataset m_dataset;
    RasterTransform m_transform;
    Mask m_mask;
    inputs::Record m_record;
};
} // namespace rf_builder::gdal
