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

#include "Dataset.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <mutex>
#include <stdexcept>

#include <gdal_priv.h>
#include <libassert/assert.hpp>
#include <ogrsf_frmts.h>

#include "ctb/Grid.hpp"
#include "init.h"
#include "log.h"
#include "srs.h"

void GDALDatasetDeleter::operator()(GDALDataset *dataset) const {
    if (dataset) {
        GDALClose(dataset);
    }
}

static GDALDataset *open_gdal_dataset(const std::filesystem::path &path, unsigned int flags) {
    initialize_gdal_once();
    const std::string path_str = path.string();
    static std::mutex gdal_open_mutex;
    const std::lock_guard lock(gdal_open_mutex);
    return static_cast<GDALDataset *>(GDALOpenEx(path_str.c_str(), flags, nullptr, nullptr, nullptr));
}

Expected<Dataset> Dataset::open_raster(std::filesystem::path path)
{
    if (GDALDataset *dataset = open_gdal_dataset(path, GDAL_OF_RASTER)) {
        return Dataset(path, dataset);
    }
    return Error::fail(Error::Code::Io, "open raster dataset", path);
}
Expected<Dataset> Dataset::open_vector(std::filesystem::path path)
{
    if (GDALDataset *dataset = open_gdal_dataset(path, GDAL_OF_VECTOR)) {
        return Dataset(path, dataset);
    }
    return Error::fail(Error::Code::Io, "open vector dataset", path);
}
Expected<std::shared_ptr<Dataset>> Dataset::open_shared_raster(std::filesystem::path path)
{
    if (GDALDataset *dataset = open_gdal_dataset(path, GDAL_OF_RASTER)) {
        return std::make_shared<Dataset>(Dataset(path, dataset));
    }
    return Error::fail(Error::Code::Io, "open raster dataset", path);
}

Dataset::Dataset(std::filesystem::path path) {
    if (GDALDataset *dataset = open_gdal_dataset(path, 0)) {
        m_path = path;
        m_gdal_dataset.reset(dataset);
    } else {
        LOG_ERROR("Failed to open dataset at path: {}\n", path.string());
        throw std::runtime_error("Failed to open dataset at path: " + path.string());
    }
}

Dataset::Dataset(GDALDataset *dataset) {
    m_gdal_dataset.reset(dataset);
    if (!m_gdal_dataset) {
        LOG_ERROR("Dataset is null.\n");
        throw std::runtime_error("Dataset is null.");
    }
}
Dataset::Dataset(const std::filesystem::path path, GDALDataset *dataset) : Dataset(dataset) {
    m_path = path;
}

Dataset Dataset::clone() {
    LOG_TRACE("Cloning dataset {}.", m_path.has_value() ? m_path->string() : "unknown");
    if (!m_path.has_value()) {
        LOG_ERROR("Cannot clone dataset.");
        throw std::runtime_error("Cannot clone dataset.");
    }
    return Dataset(this->m_path.value());
}

std::string Dataset::name() const {
    if (m_path.has_value()) {
        return m_path->stem().string();
    }
    if (m_gdal_dataset) {
        const char *name = m_gdal_dataset->GetDescription();
        if (name && strlen(name) > 0) {
            return std::string(name);
        }
        else {
            return "Anonymous";
        }
    }
    UNREACHABLE();
}

Dataset::~Dataset() = default;

Expected<radix::tile::SrsBounds> Dataset::bounds() const
{
    std::array<double, 6> adfGeoTransform = {};
    if (m_gdal_dataset->GetGeoTransform(adfGeoTransform.data()) != CE_None) {
        return Error::fail(Error::Code::Unsupported, "dataset " + name() + " has no geotransform");
    }
    if (!std::ranges::all_of(adfGeoTransform, [](double value) { return std::isfinite(value); })) {
        return Error::fail(Error::Code::InvalidInput, "dataset " + name() + " has a nonfinite geotransform");
    }

    // https://gdal.org/user/raster_data_model.html
    // gdal has a row/column raster format, where row 0 is the top most row.
    // an affine transform is used to convert row/column into the datasets SRS.
    // computing bounds is going first from row/column to dataset SRS and then to target SRS

    // we don't support sheering or rotation for now
    if (adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0) {
        return Error::fail(Error::Code::Unsupported, "dataset " + name() + " geotransform contains sheering or rotation");
    }
    // nor mirrored or south-up rasters
    if (adfGeoTransform[1] <= 0.0 || adfGeoTransform[5] >= 0.0) {
        return Error::fail(Error::Code::Unsupported, "dataset " + name() + " geotransform is not north-up with positive pixel width");
    }

    const double westX = adfGeoTransform[0];
    const double southY = adfGeoTransform[3] + (heightInPixels() * adfGeoTransform[5]);

    const double eastX = adfGeoTransform[0] + (widthInPixels() * adfGeoTransform[1]);
    const double northY = adfGeoTransform[3];
    return radix::tile::SrsBounds { { westX, southY }, { eastX, northY } };
}

Expected<radix::tile::SrsAndHeightBounds> Dataset::bounds3d(bool approx_ok) const
{
    const auto band = this->m_gdal_dataset->GetRasterBand(1);

    glm::dvec2 height_range;
    const auto result = band->GetStatistics(approx_ok, false, &height_range.x, &height_range.y, nullptr, nullptr);
    if (result != CE_None) {
        const char *unit = band->GetUnitType();
        ASSERT(unit != nullptr);
        ASSERT(strcmp(unit, "m") == 0 || strcmp(unit, "meters") == 0);
        height_range = {-11000.0, 9000.0}; // Mariana Trench and Mount Everest
    }

    auto bounds2d = this->bounds();
    if (!bounds2d) {
        return Error::propagate(std::move(bounds2d));
    }
    radix::tile::SrsAndHeightBounds bounds3d;
    bounds3d.min = glm::dvec3(bounds2d->min, height_range[0]);
    bounds3d.max = glm::dvec3(bounds2d->max, height_range[1]);
    return bounds3d;
}

Expected<std::vector<radix::tile::SrsBounds>> Dataset::geographic_coverage() const
{
    auto reference = srs();
    if (!reference) {
        return Error::propagate(std::move(reference));
    }
    auto l_bounds = bounds();
    if (!l_bounds) {
        return Error::propagate(std::move(l_bounds));
    }
    return srs::geographic_coverage(*reference, *l_bounds);
}

Expected<std::vector<radix::tile::SrsBounds>> Dataset::mercator_coverage() const
{
    auto reference = srs();
    if (!reference) {
        return Error::propagate(std::move(reference));
    }
    auto l_bounds = bounds();
    if (!l_bounds) {
        return Error::propagate(std::move(l_bounds));
    }
    return srs::mercator_coverage(*reference, *l_bounds);
}

Expected<OGRSpatialReference> Dataset::srs() const
{
    const OGRSpatialReference *source_srs = m_gdal_dataset->GetSpatialRef();
    for (int layer_index = 0; source_srs == nullptr && layer_index < m_gdal_dataset->GetLayerCount(); ++layer_index) {
        if (OGRLayer *layer = m_gdal_dataset->GetLayer(layer_index)) {
            source_srs = layer->GetSpatialRef();
        }
    }
    if (source_srs == nullptr) {
        return Error::fail(Error::Code::InvalidInput, "dataset " + name() + " does not have a spatial reference system assigned");
    }
    auto srs = *source_srs;
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return srs;
}

unsigned Dataset::widthInPixels() const {
    return ctb::i_pixel(m_gdal_dataset->GetRasterXSize());
}

unsigned Dataset::heightInPixels() const {
    return ctb::i_pixel(m_gdal_dataset->GetRasterYSize());
}

unsigned Dataset::n_bands() const {
    const auto n = m_gdal_dataset->GetRasterCount();
    DEBUG_ASSERT(n >= 0);
    return unsigned(n);
}

GDALDataset *Dataset::gdalDataset() {
    return m_gdal_dataset.get();
}
const GDALDataset *Dataset::gdalDataset() const {
    return m_gdal_dataset.get();
}
