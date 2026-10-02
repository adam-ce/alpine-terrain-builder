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

std::optional<Dataset> Dataset::open_raster(std::filesystem::path path) {
    if (GDALDataset *dataset = open_gdal_dataset(path, GDAL_OF_RASTER)) {
        return std::optional<Dataset>(Dataset(path, dataset));
    }
    LOG_ERROR("Couldn't open raster dataset {}.\n", path);
    return std::nullopt;
}
std::optional<Dataset> Dataset::open_vector(std::filesystem::path path) {
    if (GDALDataset *dataset = open_gdal_dataset(path, GDAL_OF_VECTOR)) {
        return std::optional<Dataset>(Dataset(path, dataset));
    }
    LOG_ERROR("Couldn't open vector dataset {}.\n", path);
    return std::nullopt;
}
std::optional<std::shared_ptr<Dataset>> Dataset::open_shared_raster(std::filesystem::path path) {
    if (GDALDataset *dataset = open_gdal_dataset(path, GDAL_OF_RASTER)) {
        return std::make_shared<Dataset>(Dataset(path, dataset));
    }
    LOG_ERROR("Couldn't open shared raster dataset {}.\n", path);
    return std::nullopt;
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

Expected<radix::tile::SrsBounds> Dataset::bounds(const OGRSpatialReference& targetSrs) const
{
    const auto bounds_result = bounds();
    if (!bounds_result) {
        return bounds_result;
    }
    const auto l_bounds = *bounds_result;
    const auto west = l_bounds.min.x;
    const auto east = l_bounds.max.x;
    const auto north = l_bounds.max.y;
    const auto south = l_bounds.min.y;

    auto data_srs = srs();
    if (!data_srs) {
        return Error::propagate(std::move(data_srs));
    }
    if (targetSrs.IsSame(&*data_srs))
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
        return Error::fail(Error::Code::Unsupported, "west coordinate > east coordinate");
    for (double s = west; s < east; s += deltaX) {
        addCoordinate(s, south);
        addCoordinate(s, north);
    }
    const auto deltaY = (north - south) / 2000.0;
    if (deltaY <= 0.0)
        return Error::fail(Error::Code::Unsupported, "south coordinate > north coordinate");
    for (double s = south; s < north; s += deltaY) {
        addCoordinate(west, s);
        addCoordinate(east, s);
    }
    // don't wanna miss out the max/max edge vertex
    addCoordinate(east, north);

    auto transformer = srs::transformation(*data_srs, targetSrs);
    if (!transformer) {
        return Error::propagate(std::move(transformer));
    }
    if (!(*transformer)->Transform(int(x.size()), x.data(), y.data())) {
        return Error::fail(Error::Code::InvalidInput, "transform dataset bounds to target SRS");
    }

    DEBUG_ASSERT(!x.empty());
    DEBUG_ASSERT(!y.empty());
    const double target_minX = *std::min_element(x.begin(), x.end());
    const double target_maxX = *std::max_element(x.begin(), x.end());
    const double target_minY = *std::min_element(y.begin(), y.end());
    const double target_maxY = *std::max_element(y.begin(), y.end());
    return radix::tile::SrsBounds { { target_minX, target_minY }, { target_maxX, target_maxY } };
}

Expected<std::vector<radix::tile::SrsBounds>> Dataset::geodetic_coverage() const
{
    auto reference = srs();
    if (!reference) {
        return Error::propagate(std::move(reference));
    }
    auto l_bounds = bounds();
    if (!l_bounds) {
        return Error::propagate(std::move(l_bounds));
    }
    return srs::geodetic_coverage(*reference, *l_bounds);
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

Expected<double> Dataset::widthInPixels(const radix::tile::SrsBounds& bounds, const OGRSpatialReference& bounds_srs) const
{
    return pixelWidthIn(bounds_srs).transform([&](double pixel_width) { return bounds.width() / pixel_width; });
}

Expected<double> Dataset::heightInPixels(const radix::tile::SrsBounds& bounds, const OGRSpatialReference& bounds_srs) const
{
    return pixelHeightIn(bounds_srs).transform([&](double pixel_height) { return bounds.height() / pixel_height; });
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

Expected<double> Dataset::gridResolution(const OGRSpatialReference& target_srs) const
{
    auto width = pixelWidthIn(target_srs);
    if (!width) {
        return width;
    }
    auto height = pixelHeightIn(target_srs);
    if (!height) {
        return height;
    }
    return std::min(*width, *height);
}

namespace {
Expected<radix::tile::SrsBounds> bounds_in(const Dataset& dataset, const OGRSpatialReference& target_srs)
{
    auto bounds = dataset.bounds();
    if (!bounds) {
        return bounds;
    }
    auto reference = dataset.srs();
    if (!reference) {
        return Error::propagate(std::move(reference));
    }
    return srs::non_exact_bounds_transform(*bounds, *reference, target_srs);
}
} // namespace

Expected<double> Dataset::pixelWidthIn(const OGRSpatialReference& target_srs) const
{
    return bounds_in(*this, target_srs).transform([&](const radix::tile::SrsBounds& bounds) { return bounds.width() / widthInPixels(); });
}

Expected<double> Dataset::pixelHeightIn(const OGRSpatialReference& target_srs) const
{
    return bounds_in(*this, target_srs).transform([&](const radix::tile::SrsBounds& bounds) { return bounds.height() / heightInPixels(); });
}
