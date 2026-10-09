/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek-Litofcenko
 * Copyright (C) 2022 Martin Braunsperger
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

#ifndef DATASETREADER_H
#define DATASETREADER_H

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>

#include <radix/raster.h>
#include <radix/tile.h>
#include <glm/gtc/type_precision.hpp>
#include "Error.h"
#include "RasterTransform.h"
#include "srs.h"

class Dataset;
class GDALDataset;

// Reads selected dataset bands into a target projection, using Lanczos at base
// resolution and GDAL's approximate transformer. Validity comes from the
// effective GDAL masks of the selected bands; RGB bands share one combined
// mask. Sources must be Float32 for float and Byte for RGB; there is no type
// conversion. A reader and its dataset belong to one thread at a time.
template <typename Pixel>
class DatasetReader {
    static_assert(std::is_same_v<Pixel, float> || std::is_same_v<Pixel, glm::u8vec3>, "DatasetReader supports float and glm::u8vec3 pixels");

public:
    static constexpr unsigned channel_count = std::is_same_v<Pixel, float> ? 1 : 3;

    // Data and validity have the same size. Validity is zero for invalid and
    // nonzero for valid pixels; it is not image opacity. Invalid pixels hold
    // the default pixel, which valid pixels may equal as well.
    struct Samples {
        radix::Raster<Pixel> data;
        radix::Raster<std::uint8_t> valid;
    };

    // Bands are one-based GDAL band numbers, in RGB order for colour. The
    // default pixel of a float reader must be finite.
    static Expected<DatasetReader> make(
        std::shared_ptr<Dataset> dataset, srs::Projection projection, std::array<unsigned, channel_count> bands, Pixel default_pixel);

    DatasetReader(DatasetReader&&) noexcept = default;
    DatasetReader& operator=(DatasetReader&&) noexcept = default;
    DatasetReader(const DatasetReader&) = delete;
    DatasetReader& operator=(const DatasetReader&) = delete;
    ~DatasetReader() = default;

    // Bounds are in the reader's projection. Windows without valid source
    // coverage yield default pixels and zero validity.
    Expected<Samples> read(const radix::tile::SrsBounds& bounds, glm::uvec2 size);
    // Smallest source pixel size within bounds, per axis, in units of the projection.
    // Approximate: sampled on a grid within the dataset coverage.
    Expected<glm::dvec2> min_pixel_size(const radix::tile::SrsBounds& bounds) const;

    srs::Projection projection() const { return m_projection; }

private:
    // Opaque handle of GDAL's image-to-image transformer from the output pixels to the dataset pixels.
    struct Transformer;
    struct TransformerDeleter {
        void operator()(Transformer* transformer) const;
    };

    DatasetReader(std::shared_ptr<Dataset> dataset,
        srs::Projection projection,
        std::array<unsigned, channel_count> bands,
        Pixel default_pixel,
        std::unique_ptr<Transformer, TransformerDeleter> transformer);

    std::shared_ptr<Dataset> m_dataset;
    srs::Projection m_projection;
    std::array<unsigned, channel_count> m_bands;
    Pixel m_default_pixel;
    std::unique_ptr<Transformer, TransformerDeleter> m_transformer;
};

extern template class DatasetReader<float>;
extern template class DatasetReader<glm::u8vec3>;

// The previous RF import reader, kept until the RF builder migrates to DatasetReader.
// Lanczos at base resolution, exact coordinates, declared source validity.
// RGB requires a valid filtered result in every selected channel.
namespace deprecated {
Expected<DatasetReader<float>::Samples> read_scalar(
    GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, glm::uvec2 size, unsigned band);
Expected<DatasetReader<glm::u8vec3>::Samples> read_colour(
    GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, glm::uvec2 size, const std::array<unsigned, 3>& bands);

inline Expected<DatasetReader<float>::Samples> read_scalar(
    GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, unsigned side, unsigned band)
{
    return read_scalar(dataset, transform, bounds, glm::uvec2(side), band);
}
inline Expected<DatasetReader<glm::u8vec3>::Samples> read_colour(
    GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, unsigned side, const std::array<unsigned, 3>& bands)
{
    return read_colour(dataset, transform, bounds, glm::uvec2(side), bands);
}
} // namespace deprecated

#endif // DATASETREADER_H
