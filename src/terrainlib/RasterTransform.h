/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include <array>
#include <memory>
#include <vector>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <radix/tile.h>
#include "Error.h"
#include "srs.h"

// Exact affine raster coordinates, independent of delivery-tile dimensions.
class RasterTransform {
public:
    static constexpr double world_half_extent = srs::webmercator_half_extent;
    static constexpr double latitude_limit = srs::webmercator_latitude_limit;
    using Bounds = radix::tile::SrsBounds;

    static Expected<RasterTransform> create(GDALDataset& dataset);
    static Expected<std::vector<Bounds>> coverage(const OGRSpatialReference& reference, const Bounds& bounds);

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
