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

#include <memory>
#include <string>

#include <radix/raster.h>
#include <radix/tile.h>
#include <glm/gtc/type_precision.hpp>
#include "RasterTransform.h"

class Dataset;

class DatasetReader {
public:
    enum class Projection { WebMercator, Geodetic };

    template <typename PixelType>
    struct Samples {
        radix::Raster<PixelType> data;
        radix::Raster<std::uint8_t> valid;
    };

    // RF import: Lanczos at base resolution, exact coordinates, declared source
    // validity. RGB requires a valid filtered result in every selected channel.
    static Expected<Samples<float>> read_scalar(
        GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, unsigned side, unsigned band)
    {
        return read_scalar(dataset, transform, bounds, glm::uvec2(side), band);
    }
    static Expected<Samples<glm::u8vec3>> read_colour(
        GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, unsigned side, const std::array<unsigned, 3>& bands)
    {
        return read_colour(dataset, transform, bounds, glm::uvec2(side), bands);
    }

    static Expected<Samples<float>> read_scalar(
        GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, glm::uvec2 size, unsigned band);
    static Expected<Samples<glm::u8vec3>> read_colour(
        GDALDataset& dataset, const RasterTransform& transform, const radix::tile::SrsBounds& bounds, glm::uvec2 size, const std::array<unsigned, 3>& bands);

    DatasetReader(const std::shared_ptr<Dataset>& dataset, Projection projection, unsigned band);

    radix::Raster<float> read(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const;
    // Smallest source pixel size within bounds, per axis, in units of the projection.
    // Approximate: sampled on a grid within the dataset coverage.
    Expected<glm::dvec2> min_pixel_size(const radix::tile::SrsBounds& bounds) const;

    Projection projection() const { return m_projection; }
    unsigned dataset_band() const { return m_band; }
    bool isReprojecting() const { return m_requires_reprojection; }
    std::string dataset_srs_wkt() const { return m_dataset_srs_wkt; }
    std::string target_srs_wkt() const { return m_target_srs_wkt; }

protected:
    radix::Raster<float> readFrom(const std::shared_ptr<Dataset>& dataset, const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const;

private:
    std::shared_ptr<Dataset> m_dataset;
    Projection m_projection;
    std::string m_dataset_srs_wkt;
    std::string m_target_srs_wkt;
    bool m_requires_reprojection;
    unsigned m_band;
};

#endif // DATASETREADER_H
