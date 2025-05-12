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

#include "Dataset.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <regex>
#include <stdexcept>

#include <gdal_priv.h>

#include "ctb/Grid.hpp"
#include "init.h"
#include "log.h"
#include "srs.h"

Dataset::Dataset(const std::string &path) {
    initialize_gdal_once();
    m_gdal_dataset.reset(static_cast<GDALDataset *>(GDALOpen(path.c_str(), GA_ReadOnly)));
    if (!m_gdal_dataset) {
        LOG_ERROR("Couldn't open dataset {}.\n", path);
        throw std::runtime_error("");
    }
    m_path = path;
    m_name = std::regex_replace(path, std::regex("^.*/"), "");
    m_name = std::regex_replace(m_name, std::regex(R"(\.\w+$)"), "");
}

Dataset::Dataset(GDALDataset *dataset) {
    initialize_gdal_once();
    m_gdal_dataset.reset(dataset);
    if (!m_gdal_dataset) {
        LOG_ERROR("Dataset is null.\n");
        throw std::runtime_error("Dataset is null.");
    }
}

DatasetPtr Dataset::make_shared(const std::string &path) {
    return std::make_shared<Dataset>(path);
}

Dataset Dataset::clone() {
    if (!m_path.has_value()) {
        LOG_ERROR("Cannot clone dataset.\n");
        throw std::runtime_error("Cannot clone dataset.");
    }
    return Dataset(this->m_path.value());
}

std::string Dataset::name() const {
    return m_name;
}

Dataset::~Dataset() = default;

radix::tile::SrsBounds Dataset::bounds() const {
    std::array<double, 6> adfGeoTransform = {};
    if (m_gdal_dataset->GetGeoTransform(adfGeoTransform.data()) != CE_None)
        throw std::runtime_error("Could not get transformation information from source dataset");

    // https://gdal.org/user/raster_data_model.html
    // gdal has a row/column raster format, where row 0 is the top most row.
    // an affine transform is used to convert row/column into the datasets SRS.
    // computing bounds is going first from row/column to dataset SRS and then to target SRS

    // we don't support sheering or rotation for now
    if (adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0)
        throw std::runtime_error("Dataset geo transform contains sheering or rotation. This is not supported!");

    const double westX = adfGeoTransform[0];
    const double southY = adfGeoTransform[3] + (heightInPixels() * adfGeoTransform[5]);

    const double eastX = adfGeoTransform[0] + (widthInPixels() * adfGeoTransform[1]);
    const double northY = adfGeoTransform[3];
    return {{westX, southY}, {eastX, northY}};
}

radix::tile::SrsAndHeightBounds Dataset::bounds3d() const {
    const auto band = this->m_gdal_dataset->GetRasterBand(1);

    glm::dvec2 height_range;
    if (!band->GetStatistics(1, 0, &height_range.x, &height_range.y, nullptr, nullptr)) {
        throw std::runtime_error("Could not determine dataset height bounds");
    }

    const auto bounds2d = this->bounds();
    radix::tile::SrsAndHeightBounds bounds3d;
    bounds3d.min = glm::dvec3(bounds2d.min, height_range[0]);
    bounds3d.max = glm::dvec3(bounds2d.max, height_range[1]);
    return bounds3d;
}

radix::tile::SrsBounds Dataset::bounds(const OGRSpatialReference &targetSrs) const {
    const auto l_bounds = bounds();
    const auto west = l_bounds.min.x;
    const auto east = l_bounds.max.x;
    const auto north = l_bounds.max.y;
    const auto south = l_bounds.min.y;

    const auto data_srs = srs();
    if (targetSrs.IsSame(&data_srs))
        return l_bounds;

    // We need to transform the bounds to the target SRS
    // this might involve warping, i.e. some of the edges can be arcs.
    // therefore we want to walk the perimiter and get min/max from there.
    // a resolution of 2000 samples per border should give a good enough approximation.

    // hey, check out inline virtual int TransformBounds(const double xmin, const double ymin, const double xmax, const double ymax, double *out_xmin, double *out_ymin, double *out_xmax, double *out_ymax, const int densify_pts)
    std::vector<double> x;
    std::vector<double> y;
    auto addCoordinate = [&](double xv, double yv) { x.emplace_back(xv); y.emplace_back(yv); };

    const auto deltaX = l_bounds.width() / 2000.0;
    if (deltaX <= 0.0)
        throw std::runtime_error("west coordinate > east coordinate. This is not supported.");
    for (double s = west; s < east; s += deltaX) {
        addCoordinate(s, south);
        addCoordinate(s, north);
    }
    const auto deltaY = (north - south) / 2000.0;
    if (deltaY <= 0.0)
        throw std::runtime_error("south coordinate > north coordinate. This is not supported.");
    for (double s = south; s < north; s += deltaY) {
        addCoordinate(west, s);
        addCoordinate(east, s);
    }
    // don't wanna miss out the max/max edge vertex
    addCoordinate(east, north);

    const auto transformer = srs::transformation(srs(), targetSrs);
    if (!transformer->Transform(int(x.size()), x.data(), y.data())) {
        throw std::string("Could not transform dataset bounds to target SRS");
    }

    assert(!x.empty());
    assert(!y.empty());
    const double target_minX = *std::min_element(x.begin(), x.end());
    const double target_maxX = *std::max_element(x.begin(), x.end());
    const double target_minY = *std::min_element(y.begin(), y.end());
    const double target_maxY = *std::max_element(y.begin(), y.end());
    return {{target_minX, target_minY}, {target_maxX, target_maxY}};
}

OGRSpatialReference Dataset::srs() const {
    const char *srcWKT = m_gdal_dataset->GetProjectionRef();
    if (!strlen(srcWKT))
        throw std::runtime_error("The source dataset does not have a spatial reference system assigned");
    auto srs = OGRSpatialReference(srcWKT);
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return srs;
}

unsigned Dataset::widthInPixels() const {
    return ctb::i_pixel(m_gdal_dataset->GetRasterXSize());
}

unsigned Dataset::heightInPixels() const {
    return ctb::i_pixel(m_gdal_dataset->GetRasterYSize());
}

double Dataset::widthInPixels(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &bounds_srs) const {
    return bounds.width() / pixelWidthIn(bounds_srs);
}

double Dataset::heightInPixels(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &bounds_srs) const {
    return bounds.height() / pixelHeightIn(bounds_srs);
}

unsigned Dataset::n_bands() const {
    const auto n = m_gdal_dataset->GetRasterCount();
    assert(n >= 0);
    return unsigned(n);
}

GDALDataset *Dataset::gdalDataset() {
    return m_gdal_dataset.get();
}

double Dataset::gridResolution(const OGRSpatialReference &target_srs) const {
    return std::min(pixelWidthIn(target_srs), pixelHeightIn(target_srs));
}

double Dataset::pixelWidthIn(const OGRSpatialReference &target_srs) const {
    const auto b0 = bounds();
    const auto b1 = srs::non_exact_bounds_transform(b0, srs(), target_srs);
    return b1.width() / widthInPixels();
}

double Dataset::pixelHeightIn(const OGRSpatialReference &target_srs) const {
    const auto b = srs::non_exact_bounds_transform(bounds(), srs(), target_srs);
    return b.height() / heightInPixels();
}
