#include "RasterTransform.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include "srs.h"

namespace {
OGRSpatialReference mercator_reference()
{
    OGRSpatialReference result;
    result.importFromEPSG(3857);
    result.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return result;
}
}

Expected<std::vector<RasterTransform::Bounds>> RasterTransform::coverage(
    const OGRSpatialReference& reference, const Bounds& bounds)
{
    const auto mercator = mercator_reference();
    if (reference.IsSame(&mercator) && bounds.min.x >= -world_half_extent && bounds.max.x <= world_half_extent) {
        Bounds clipped { glm::max(bounds.min, glm::dvec2(-world_half_extent)),
            glm::min(bounds.max, glm::dvec2(world_half_extent)) };
        if (clipped.min.x >= clipped.max.x || clipped.min.y >= clipped.max.y) { return std::vector<Bounds> {}; }
        return std::vector<Bounds> { clipped };
    }
    auto geographic = srs::wgs84();
    std::unique_ptr<OGRCoordinateTransformation> transform(OGRCreateCoordinateTransformation(&reference, &geographic));
    if (!transform) {
        return Error::fail(Error::Code::InvalidInput, "create coverage transformation");
    }
    Bounds longitude_latitude;
    constexpr int boundary_samples = 257;
    const bool reprojected = !reference.IsSame(&geographic);
    if (reference.IsSame(&geographic)) {
        longitude_latitude = bounds;
    } else if (!transform->TransformBounds(bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y,
                   &longitude_latitude.min.x, &longitude_latitude.min.y,
                   &longitude_latitude.max.x, &longitude_latitude.max.y, boundary_samples)) {
        return Error::fail(Error::Code::InvalidInput, "transform source coverage bounds");
    }
    const double south = (std::max)(-latitude_limit, longitude_latitude.min.y);
    const double north = (std::min)(latitude_limit, longitude_latitude.max.y);
    if (south >= north) {
        return std::vector<Bounds> {};
    }
    double west = longitude_latitude.min.x;
    double east = longitude_latitude.max.x;
    if (!std::isfinite(west) || !std::isfinite(east) || !std::isfinite(south) || !std::isfinite(north)) {
        return Error::fail(Error::Code::InvalidInput, "nonfinite source coverage bounds");
    }
    if (east < west) {
        east += 360;
    }
    // Densified transformation samples are not exact extrema of curved edges.
    // Retain a full sampling interval as a guard band rather than pruning at
    // the sampled extremum. Final pixel acceptance uses original-CRS geometry.
    const double longitude_guard = reprojected ? (std::max)(1e-6, (east - west) / (boundary_samples + 1)) : 0;
    const double latitude_guard = reprojected ? (std::max)(1e-6,
        (longitude_latitude.max.y - longitude_latitude.min.y) / (boundary_samples + 1)) : 0;
    west -= longitude_guard;
    east += longitude_guard;
    if (east - west >= 360) {
        west = -180;
        east = 180;
    } else {
        const double shift = 360 * std::floor((west + 180) / 360);
        west -= shift;
        east -= shift;
    }
    const auto latitude_y = [](double latitude) {
        return std::clamp(world_half_extent / std::numbers::pi
                * std::asinh(std::tan(latitude * std::numbers::pi / 180)),
            -world_half_extent, world_half_extent);
    };
    const auto rectangle = [&](double left, double right) -> Bounds {
        return { { left * world_half_extent / 180, latitude_y((std::max)(-latitude_limit, south - latitude_guard)) },
            { right * world_half_extent / 180, latitude_y((std::min)(latitude_limit, north + latitude_guard)) } };
    };
    if (east > 180) {
        return std::vector<Bounds> { rectangle(west, 180), rectangle(-180, east - 360) };
    }
    return std::vector<Bounds> { rectangle(west, east) };
}

Expected<RasterTransform> RasterTransform::create(GDALDataset& dataset)
{
    RasterTransform result;
    const auto* reference = dataset.GetSpatialRef();
    if (!reference || dataset.GetGeoTransform(result.m_affine.data()) != CE_None
        || dataset.GetGCPCount() != 0 || CSLCount(dataset.GetMetadata("RPC")) != 0
        || CSLCount(dataset.GetMetadata("GEOLOCATION")) != 0) {
        return Error::fail(Error::Code::Unsupported, "RF inputs require an explicit CRS and affine geotransform; GCP/RPC/geolocation inputs are unsupported");
    }
    if (dataset.GetRasterXSize() <= 0 || dataset.GetRasterYSize() <= 0
        || !std::ranges::all_of(result.m_affine, [](double value) { return std::isfinite(value); })
        || !GDALInvGeoTransform(result.m_affine.data(), result.m_inverse.data())) {
        return Error::fail(Error::Code::InvalidInput, "invalid raster dimensions or singular affine geotransform");
    }
    result.m_reference = *reference;
    result.m_reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    const auto mercator = mercator_reference();
    result.m_to_source.reset(OGRCreateCoordinateTransformation(&mercator, &result.m_reference));
    result.m_to_mercator.reset(OGRCreateCoordinateTransformation(&result.m_reference, &mercator));
    if (!result.m_to_source || !result.m_to_mercator) {
        return Error::fail(Error::Code::InvalidInput, "create affine raster CRS transformations");
    }
    if (result.m_reference.IsGeographic()) {
        result.m_period = 2 * std::numbers::pi / result.m_reference.GetAngularUnits();
    } else if (result.m_reference.IsSame(&mercator)) {
        result.m_period = 2 * world_half_extent;
    }
    Bounds bounds { { INFINITY, INFINITY }, { -INFINITY, -INFINITY } };
    for (double column : { 0., double(dataset.GetRasterXSize()) }) {
        for (double row : { 0., double(dataset.GetRasterYSize()) }) {
            glm::dvec2 point;
            GDALApplyGeoTransform(result.m_affine.data(), column, row, &point.x, &point.y);
            bounds.min = glm::min(bounds.min, point);
            bounds.max = glm::max(bounds.max, point);
        }
    }
    result.m_centre_x = (bounds.min.x + bounds.max.x) / 2;
    auto coverage_bounds = coverage(result.m_reference, bounds);
    if (!coverage_bounds) {
        return Error::propagate(std::move(coverage_bounds));
    }
    result.m_bounds = std::move(*coverage_bounds);
    return result;
}

RasterTransform::Bounds RasterTransform::tile_bounds(const radix::tile::Id& key)
{
    // ldexp avoids both 1u << 32 and tile-coordinate * tile-dimension overflow.
    const double side = std::ldexp(2 * world_half_extent, -int(key.zoom_level));
    return { { -world_half_extent + double(key.coords.x) * side,
                 world_half_extent - (double(key.coords.y) + 1) * side },
        { -world_half_extent + (double(key.coords.x) + 1) * side,
            world_half_extent - double(key.coords.y) * side } };
}

Expected<glm::dvec2> RasterTransform::source_pixel(glm::dvec2 point) const
{
    if (!m_to_source->Transform(1, &point.x, &point.y) || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return Error::fail(Error::Code::InvalidInput, "transform RF position into source CRS");
    }
    // Keep a seam-crossing affine grid continuous in its own source coordinates.
    // This is source addressing only; RF coordinates remain canonical.
    if (m_period > 0) {
        point.x = m_centre_x + std::remainder(point.x - m_centre_x, m_period);
    }
    glm::dvec2 pixel;
    GDALApplyGeoTransform(const_cast<double*>(m_inverse.data()), point.x, point.y, &pixel.x, &pixel.y);
    return pixel;
}

Expected<glm::dvec2> RasterTransform::mercator(glm::dvec2 pixel) const
{
    glm::dvec2 point;
    GDALApplyGeoTransform(const_cast<double*>(m_affine.data()), pixel.x, pixel.y, &point.x, &point.y);
    if (!m_to_mercator->Transform(1, &point.x, &point.y) || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return Error::fail(Error::Code::InvalidInput, "transform source pixel into RF CRS");
    }
    return point;
}
