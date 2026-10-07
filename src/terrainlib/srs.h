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

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <expected>
#include <fmt/format.h>
#include <glm/detail/qualifier.hpp>
#include <glm/glm.hpp>
#include <ogr_spatialref.h>
#include <radix/geometry.h>
#include <radix/tile.h>

#include "Error.h"

namespace srs {

std::optional<int> epsg_code(const OGRSpatialReference& srs);

std::string friendly_name(const OGRSpatialReference& srs);

std::unique_ptr<OGRSpatialReference> clone(const OGRSpatialReference& srs);

Expected<std::unique_ptr<OGRCoordinateTransformation>> uncached_transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs);

Expected<std::shared_ptr<OGRCoordinateTransformation>> transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs);

template <typename T>
inline Expected<glm::tvec2<T>> transform_point(OGRCoordinateTransformation* transform, glm::tvec2<T> p)
{
    if (!transform->Transform(1, &p.x, &p.y))
        return Error::fail(Error::Code::InvalidInput, "transform 2d point");
    return p;
}
template <typename T>
inline Expected<glm::tvec3<T>> transform_point(OGRCoordinateTransformation* transform, glm::tvec3<T> p)
{
    if (!transform->Transform(1, &p.x, &p.y, &p.z))
        return Error::fail(Error::Code::InvalidInput, "transform 3d point");
    return p;
}

template <typename T>
inline Expected<glm::tvec2<T>> transform_point(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, glm::tvec2<T> p)
{
    auto transform = transformation(source_srs, target_srs);
    if (!transform)
        return Error::propagate(std::move(transform));
    return transform_point(transform->get(), p);
}
template <typename T>
inline Expected<glm::tvec3<T>> transform_point(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, glm::tvec3<T> p)
{
    auto transform = transformation(source_srs, target_srs);
    if (!transform)
        return Error::propagate(std::move(transform));
    return transform_point(transform->get(), p);
}

template <typename Container>
inline Expected<void> transform_points_inplace(OGRCoordinateTransformation* transform, Container& points)
{
    using PointType = typename Container::value_type;
    using T = typename PointType::value_type;
    constexpr bool is_3d = (PointType::length() == 3);
    constexpr bool is_array = std::is_array_v<Container>;

    const size_t size = points.size();

    constexpr std::size_t array_size = is_array ? points.size() : 0;
    using OutputContainer = std::conditional_t<is_array, std::array<T, array_size>, std::vector<T>>;

    OutputContainer xs, ys;
    std::conditional_t<is_3d, OutputContainer, int> zs;

    if constexpr (!is_array) {
        xs.resize(size);
        ys.resize(size);
        if constexpr (PointType::length() == 3) {
            zs.resize(size);
        }
    }

    for (size_t i = 0; i < size; i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
        if constexpr (is_3d) {
            zs[i] = points[i].z;
        }
    }

    bool success;
    if constexpr (is_3d) {
        success = transform->Transform(size, xs.data(), ys.data(), zs.data());
    } else {
        success = transform->Transform(size, xs.data(), ys.data());
    }

    if (!success) {
        return Error::fail(Error::Code::InvalidInput, "transform points");
    }

    for (size_t i = 0; i < size; i++) {
        if constexpr (is_3d) {
            points[i] = PointType(xs[i], ys[i], zs[i]);
        } else {
            points[i] = PointType(xs[i], ys[i]);
        }
    }
    return {};
}

template <typename Container>
inline Expected<Container> transform_points(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, Container points)
{
    auto transform = transformation(source_srs, target_srs);
    if (!transform)
        return Error::propagate(std::move(transform));
    if (auto result = transform_points_inplace(transform->get(), points); !result)
        return Error::propagate(std::move(result));
    return points;
}

// TODO: somehow integrate into a single transform_points
template <typename T>
inline Expected<std::vector<glm::tvec2<T>>> transform_points_to_2d(OGRCoordinateTransformation* transform, const std::vector<glm::tvec3<T>>& points)
{
    std::vector<T> xs;
    std::vector<T> ys;
    std::vector<T> zs;

    xs.resize(points.size());
    ys.resize(points.size());
    zs.resize(points.size());

    for (size_t i = 0; i < points.size(); i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
        zs[i] = points[i].z;
    }

    if (!transform->Transform(points.size(), xs.data(), ys.data(), zs.data())) {
        return Error::fail(Error::Code::InvalidInput, "transform points to 2d");
    }

    std::vector<glm::tvec2<T>> transformed;
    transformed.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        transformed[i] = { xs[i], ys[i] };
    }

    return transformed;
}

std::expected<OGRSpatialReference, std::string> from_epsg(const uint32_t epsg);

std::expected<OGRSpatialReference, std::string> from_user_input(const std::string& user_input);

inline OGRSpatialReference ecef() { return from_epsg(4978).value(); }
inline OGRSpatialReference webmercator() { return from_epsg(3857).value(); }
inline OGRSpatialReference wgs84() { return from_epsg(4326).value(); }
inline OGRSpatialReference mgi() { return from_epsg(4312).value(); }

/// Half the side length of the square Web Mercator world in metres.
constexpr double webmercator_half_extent = 20037508.342789244;
/// Latitude in degrees at which Web Mercator y reaches webmercator_half_extent.
constexpr double webmercator_latitude_limit = 85.0511287798066;
/// Web Mercator bounds of an XYZ tile; valid up to zoom level 32.
radix::tile::SrsBounds webmercator_tile_bounds(const radix::tile::Id& key);

/// WGS84 longitude/latitude bounds in degrees covering the given bounds.
/// Longitudes lie in [-180, 180]; coverage crossing the antimeridian is split into two bounds.
/// Reprojected coverage includes a guard band, because edge sampling can miss extrema of curved edges.
Expected<std::vector<radix::tile::SrsBounds>> geographic_coverage(const OGRSpatialReference& reference, const radix::tile::SrsBounds& bounds);
/// Web Mercator bounds covering the given bounds within the polar latitude limits.
/// Returns zero (polar-only coverage), one, or two (crossing the antimeridian) bounds.
Expected<std::vector<radix::tile::SrsBounds>> mercator_coverage(const OGRSpatialReference& reference, const radix::tile::SrsBounds& bounds);
/// ECEF bounds covering the given bounds of a 2D SRS, whose z is ellipsoidal height.
/// Goes to longitude/latitude via geographic_coverage, then analytically to ECEF:
/// within a longitude/latitude/height box, every ECEF coordinate is monotonic along each
/// axis, except at longitudes -90, 0, 90 and the equator. Evaluating all combinations of
/// the box limits and the turning values inside it gives the exact ECEF bounds of that box.
/// Compound SRSes are unsupported.
Expected<radix::geometry::Aabb3d> ecef_coverage(const OGRSpatialReference& reference, const radix::geometry::Aabb3d& bounds);
/// Bounds in a 2D SRS covering the given ECEF bounds; z is ellipsoidal height.
/// Goes analytically to longitude/latitude first: longitude from the corners of the xy
/// rectangle, latitude and height from a few points given by the distance to the polar axis
/// and z. WGS84 and Web Mercator results are quasi exact (as tight as it gets using proj); other SRSes use GDAL's densified
/// boundary transformation with a guard band. Geographic bounds are split at the
/// antimeridian, so usually there is one bounds, or two when crossing it; Web Mercator
/// returns none outside its latitude limits.
/// Fails for compound SRSes, for bounds close to the earth's centre, where PROJ is
/// erratic, and if the transformation fails.
Expected<std::vector<radix::geometry::Aabb3d>> ecef2srs_coverage(const radix::geometry::Aabb3d& bounds, const OGRSpatialReference& reference);

} // namespace srs
