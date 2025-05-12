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

#ifndef DATASET_H
#define DATASET_H

#include <memory>
#include <string>

#include <radix/tile.h>

class GDALDataset;
class OGRSpatialReference;
class OGRCoordinateTransformation;

namespace ctb {
class Grid;
}

class Dataset;
using DatasetPtr = std::shared_ptr<Dataset>;

class Dataset {
public:
    Dataset(const std::string& path);
    Dataset(GDALDataset* dataset); // takes over ownership
    ~Dataset();
    static DatasetPtr make_shared(const std::string &path);
    Dataset clone();

    [[nodiscard]] std::string name() const;

    [[nodiscard]] radix::tile::SrsBounds bounds() const;
    [[nodiscard]] radix::tile::SrsAndHeightBounds bounds3d() const;
    [[nodiscard]] radix::tile::SrsBounds bounds(const OGRSpatialReference &targetSrs) const;
    [[nodiscard]] OGRSpatialReference srs() const;
    [[nodiscard]] unsigned int widthInPixels() const;
    [[nodiscard]] unsigned int heightInPixels() const;
    [[nodiscard]] double widthInPixels(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &bounds_srs) const;
    [[nodiscard]] double heightInPixels(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &bounds_srs) const;
    [[nodiscard]] unsigned int n_bands() const;
    [[nodiscard]] GDALDataset *gdalDataset();

    [[nodiscard]] double gridResolution(const OGRSpatialReference &target_srs) const;
    [[nodiscard]] double pixelWidthIn(const OGRSpatialReference &target_srs) const;
    [[nodiscard]] double pixelHeightIn(const OGRSpatialReference &target_srs) const;

private:
    std::unique_ptr<GDALDataset> m_gdal_dataset;
    std::optional<std::string> m_path;
    std::string m_name;
    };

#endif
