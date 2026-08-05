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

#ifndef DATASETREADER_H
#define DATASETREADER_H

#include <memory>
#include <string>

#include "Image.h"
#include <radix/tile.h>

class Dataset;
class OGRSpatialReference;

class DatasetReader {
public:
    DatasetReader(const std::shared_ptr<Dataset>& dataset, const OGRSpatialReference& targetSRS, unsigned band, bool warn_on_missing_overviews = true);

    HeightData read(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const;
    HeightData readWithOverviews(const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const;

    unsigned dataset_band() const { return m_band; }
    bool isReprojecting() const { return m_requires_reprojection; }
    std::string dataset_srs_wkt() const { return m_dataset_srs_wkt; }
    std::string target_srs_wkt() const { return m_target_srs_wkt; }

protected:
    HeightData readFrom(const std::shared_ptr<Dataset>& dataset, const radix::tile::SrsBounds& bounds, unsigned width, unsigned height) const;

private:
    std::shared_ptr<Dataset> m_dataset;
    std::string m_dataset_srs_wkt;
    std::string m_target_srs_wkt;
    bool m_requires_reprojection;
    [[maybe_unused]] bool m_warn_on_missing_overviews;
    unsigned m_band;
};

#endif // DATASETREADER_H
