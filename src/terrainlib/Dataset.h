/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include <memory>
#include <string>
#include <optional>
#include <filesystem>

#include <radix/tile.h>
#include "log.h"

class GDALDataset;
class OGRSpatialReference;

struct GdalDatasetDeleter {
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

    [[nodiscard]] radix::tile::SrsBounds bounds() const;
    [[nodiscard]] radix::tile::SrsAndHeightBounds bounds3d(bool approx_ok = false) const;
    [[nodiscard]] radix::tile::SrsBounds bounds(const OGRSpatialReference &targetSrs) const;
    [[nodiscard]] OGRSpatialReference srs() const;
    [[nodiscard]] unsigned int widthInPixels() const;
    [[nodiscard]] unsigned int heightInPixels() const;
    [[nodiscard]] double widthInPixels(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &bounds_srs) const;
    [[nodiscard]] double heightInPixels(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &bounds_srs) const;
    [[nodiscard]] unsigned int n_bands() const;
    [[nodiscard]] GDALDataset *gdalDataset();
    [[nodiscard]] const GDALDataset *gdalDataset() const;

    [[nodiscard]] double gridResolution(const OGRSpatialReference &target_srs) const;
    [[nodiscard]] double pixelWidthIn(const OGRSpatialReference &target_srs) const;
    [[nodiscard]] double pixelHeightIn(const OGRSpatialReference &target_srs) const;

private:
    Dataset(const std::filesystem::path path, GDALDataset *dataset);
    std::unique_ptr<GDALDataset, GdalDatasetDeleter> m_gdal_dataset;
    std::optional<std::filesystem::path> m_path;
};
