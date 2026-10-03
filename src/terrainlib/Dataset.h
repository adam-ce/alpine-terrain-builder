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

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Error.h"
#include "log.h"
#include <radix/tile.h>

class GDALDataset;
class OGRSpatialReference;

struct GDALDatasetDeleter {
    void operator()(GDALDataset *dataset) const;
};

class Dataset {
public:
    Dataset(std::filesystem::path path);
    Dataset(GDALDataset* dataset); // takes over ownership
    ~Dataset();
    static std::optional<Dataset> open_raster(std::filesystem::path path);
    static std::optional<Dataset> open_vector(std::filesystem::path path);
    static std::optional<std::shared_ptr<Dataset>> open_shared_raster(std::filesystem::path path);
    Dataset clone();

    Dataset(Dataset &&) noexcept = default;
    Dataset &operator=(Dataset &&) noexcept = default;

    Dataset(const Dataset &) = delete;
    Dataset &operator=(const Dataset &) = delete;

    [[nodiscard]] std::string name() const;

    // Bounds of a north-up raster without rotation or shear, in its own SRS.
    [[nodiscard]] Expected<radix::tile::SrsBounds> bounds() const;
    [[nodiscard]] Expected<radix::tile::SrsAndHeightBounds> bounds3d(bool approx_ok = false) const;
    // See srs::geodetic_coverage and srs::mercator_coverage.
    [[nodiscard]] Expected<std::vector<radix::tile::SrsBounds>> geodetic_coverage() const;
    [[nodiscard]] Expected<std::vector<radix::tile::SrsBounds>> mercator_coverage() const;
    [[nodiscard]] Expected<OGRSpatialReference> srs() const;
    [[nodiscard]] unsigned int widthInPixels() const;
    [[nodiscard]] unsigned int heightInPixels() const;
    [[nodiscard]] Expected<double> widthInPixels(const radix::tile::SrsBounds& bounds, const OGRSpatialReference& bounds_srs) const;
    [[nodiscard]] Expected<double> heightInPixels(const radix::tile::SrsBounds& bounds, const OGRSpatialReference& bounds_srs) const;
    [[nodiscard]] unsigned int n_bands() const;
    [[nodiscard]] GDALDataset *gdalDataset();
    [[nodiscard]] const GDALDataset *gdalDataset() const;

    [[nodiscard]] Expected<double> gridResolution(const OGRSpatialReference& target_srs) const;
    [[nodiscard]] Expected<double> pixelWidthIn(const OGRSpatialReference& target_srs) const;
    [[nodiscard]] Expected<double> pixelHeightIn(const OGRSpatialReference& target_srs) const;

private:
    Dataset(const std::filesystem::path path, GDALDataset *dataset);
    std::unique_ptr<GDALDataset, GDALDatasetDeleter> m_gdal_dataset;
    std::optional<std::filesystem::path> m_path;
};
