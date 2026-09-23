#pragma once

#include <array>
#include <memory>
#include <vector>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <radix/tile.h>
#include "Error.h"

// Exact affine raster coordinates, independent of delivery-tile dimensions.
class RasterTransform {
public:
    static constexpr double world_half_extent = 20037508.342789244;
    static constexpr double latitude_limit = 85.0511287798066;
    using Bounds = radix::tile::SrsBounds;

    static Expected<RasterTransform> create(GDALDataset& dataset);
    static Expected<std::vector<Bounds>> coverage(const OGRSpatialReference& reference, const Bounds& bounds);
    static Bounds tile_bounds(const radix::tile::Id& key);

    // Sampling estimates can omit source-branch wrapping to keep derivatives
    // continuous at a global raster's longitude seam.
    Expected<glm::dvec2> source_pixel(glm::dvec2 mercator, bool wrap_longitude = true) const;
    Expected<glm::dvec2> mercator(glm::dvec2 source_pixel) const;
    const std::vector<Bounds>& bounds() const { return m_bounds; }
    const OGRSpatialReference& reference() const { return m_reference; }
    const std::array<double, 6>& affine() const { return m_affine; }

private:
    OGRSpatialReference m_reference;
    std::array<double, 6> m_affine {};
    std::array<double, 6> m_inverse {};
    std::unique_ptr<OGRCoordinateTransformation> m_to_source;
    std::unique_ptr<OGRCoordinateTransformation> m_to_mercator;
    std::vector<Bounds> m_bounds;
    double m_period = 0;
    double m_centre_x = 0;
};
