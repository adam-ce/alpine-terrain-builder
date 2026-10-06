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

#include "srs.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <limits>

namespace srs {

std::optional<int> epsg_code(const OGRSpatialReference& srs)
{
    OGRSpatialReference tmp = srs;
    tmp.AutoIdentifyEPSG();

    const char* authName = tmp.GetAuthorityName(nullptr);
    const char* authCode = tmp.GetAuthorityCode(nullptr);

    if (authName == nullptr || authCode == nullptr) {
        return std::nullopt;
    }

    if (std::string(authName) != "EPSG") {
        return std::nullopt;
    }

    return std::atoi(authCode);
}

std::string friendly_name(const OGRSpatialReference& srs)
{
    if (const auto code = epsg_code(srs)) {
        return "EPSG:" + std::to_string(*code);
    }

    const char* name = srs.GetName();

    if (name != nullptr && *name != '\0') {
        return std::string(name);
    }

    return "unknown";
}

std::unique_ptr<OGRSpatialReference> clone(const OGRSpatialReference& srs)
{
    auto cloned = srs.Clone();
    return std::unique_ptr<OGRSpatialReference>(cloned);
}

namespace {
    struct TransformationCacheEntry {
        OGRSpatialReference source;
        OGRSpatialReference target;
        std::shared_ptr<OGRCoordinateTransformation> transform;
    };
} // namespace

Expected<std::unique_ptr<OGRCoordinateTransformation>> uncached_transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs)
{
    std::unique_ptr<OGRCoordinateTransformation> transform(OGRCreateCoordinateTransformation(&source_srs, &target_srs));
    if (!transform) {
        return Error::fail(Error::Code::InvalidInput, "create SRS transformation from " + friendly_name(source_srs) + " to " + friendly_name(target_srs));
    }
    return transform;
}

Expected<std::shared_ptr<OGRCoordinateTransformation>> transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs)
{
    thread_local std::vector<TransformationCacheEntry> cache;

    // Check forward
    for (auto& entry : cache) {
        if (entry.source.IsSame(&source_srs) && entry.target.IsSame(&target_srs)) {
            return entry.transform;
        }
    }

    // Check inverse
    for (auto& entry : cache) {
        if (entry.source.IsSame(&target_srs) && entry.target.IsSame(&source_srs)) {
            std::shared_ptr<OGRCoordinateTransformation> inverse(entry.transform->GetInverse());
            if (!inverse) {
                return Error::fail(
                    Error::Code::InvalidInput, "create inverse SRS transformation from " + friendly_name(source_srs) + " to " + friendly_name(target_srs));
            }
            cache.push_back({ source_srs, target_srs, inverse });
            return inverse;
        }
    }

    // Not found
    auto transform = uncached_transformation(source_srs, target_srs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    std::shared_ptr<OGRCoordinateTransformation> shared_transform = std::move(*transform);
    cache.push_back({ source_srs, target_srs, shared_transform });
    return shared_transform;
}

Expected<radix::tile::SrsBounds> non_exact_bounds_transform(OGRCoordinateTransformation* transform, const radix::tile::SrsBounds& bounds)
{
    std::array xs = { bounds.min.x, bounds.max.x };
    std::array ys = { bounds.min.y, bounds.max.y };
    if (!transform->Transform(2, xs.data(), ys.data())) {
        return Error::fail(Error::Code::InvalidInput, "transform bounds corners");
    }
    return radix::tile::SrsBounds { { std::min(xs[0], xs[1]), std::min(ys[0], ys[1]) }, { std::max(xs[0], xs[1]), std::max(ys[0], ys[1]) } };
}

Expected<radix::tile::SrsBounds> non_exact_bounds_transform(
    const radix::tile::SrsBounds& bounds, const OGRSpatialReference& sourceSrs, const OGRSpatialReference& targetSrs)
{
    auto transform = transformation(sourceSrs, targetSrs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    return non_exact_bounds_transform(transform->get(), bounds);
}

Expected<radix::geometry::Aabb3d> non_exact_bounds_transform(OGRCoordinateTransformation* transform, const radix::geometry::Aabb3d& bounds)
{
    std::array xs = { bounds.min.x, bounds.max.x };
    std::array ys = { bounds.min.y, bounds.max.y };
    std::array zs = { bounds.min.z, bounds.max.z };
    if (!transform->Transform(2, xs.data(), ys.data(), zs.data())) {
        return Error::fail(Error::Code::InvalidInput, "transform bounds corners");
    }
    return radix::geometry::Aabb3d { { xs[0], ys[0], zs[0] }, { xs[1], ys[1], zs[1] } };
}

Expected<radix::geometry::Aabb3d> non_exact_bounds_transform(
    const radix::geometry::Aabb3d& bounds, const OGRSpatialReference& sourceSrs, const OGRSpatialReference& targetSrs)
{
    auto transform = transformation(sourceSrs, targetSrs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    return non_exact_bounds_transform(transform->get(), bounds);
}

Expected<radix::tile::SrsBounds> encompassing_bounds_transfer(OGRCoordinateTransformation* transform, const radix::tile::SrsBounds& source_bounds)
{
    radix::tile::SrsBounds target_bounds;
    const int result = transform->TransformBounds(source_bounds.min.x,
        source_bounds.min.y,
        source_bounds.max.x,
        source_bounds.max.y,
        &target_bounds.min.x,
        &target_bounds.min.y,
        &target_bounds.max.x,
        &target_bounds.max.y,
        21);
    if (result != TRUE) {
        return Error::fail(Error::Code::InvalidInput, "transform bounds");
    }
    return target_bounds;
}

Expected<radix::tile::SrsBounds> encompassing_bounds_transfer(
    const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, const radix::tile::SrsBounds& source_bounds)
{
    if (source_srs.IsSame(&target_srs)) {
        return source_bounds;
    }

    auto transformation = srs::transformation(source_srs, target_srs);
    if (!transformation) {
        return Error::propagate(std::move(transformation));
    }
    return encompassing_bounds_transfer(transformation->get(), source_bounds);
}

Expected<radix::geometry::Aabb3d> encompassing_bounds_transfer(OGRCoordinateTransformation* transform,
    const radix::geometry::Aabb3d& source_bounds,
    const uint32_t intermediate_points_edges,
    const uint32_t intermediate_points_faces)
{
    std::vector<glm::dvec3> points;

    // Add corner points
    const auto corners = radix::geometry::corners(source_bounds);
    std::copy(corners.begin(), corners.end(), std::back_inserter(points));

    // Sample points on the edges
    const auto edges = radix::geometry::edges(source_bounds);
    for (const auto& edge : edges) {
        const auto& [p0, p1] = edge;

        for (uint32_t i = 1; i <= intermediate_points_edges; i++) {
            const double t = static_cast<double>(i) / (intermediate_points_edges + 1);
            const auto p = glm::mix(p0, p1, t);
            points.push_back(p);
        }
    }

    // TODO: do we need this?
    // Sample points on the faces
    const auto quads = radix::geometry::quads(source_bounds);
    for (const auto& quad : quads) {
        const auto& [p0, p1, p2, p3] = quad;

        for (uint32_t i = 1; i <= intermediate_points_faces; i++) {
            const double u = static_cast<double>(i) / (intermediate_points_faces + 1);
            const auto edge_p0 = glm::mix(p0, p1, u);
            const auto edge_p1 = glm::mix(p3, p2, u);

            for (uint32_t j = 1; j <= intermediate_points_faces; j++) {
                const double v = static_cast<double>(j) / (intermediate_points_faces + 1);
                const auto point = glm::mix(edge_p0, edge_p1, v);
                points.push_back(point);
            }
        }
    }

    // Transform all collected points
    if (auto result = transform_points_inplace(transform, points); !result) {
        return Error::propagate(std::move(result));
    }

    // Compute bounds from transformed points
    radix::geometry::Aabb3d target_bounds;
    target_bounds.min = glm::dvec3(std::numeric_limits<double>::max());
    target_bounds.max = glm::dvec3(std::numeric_limits<double>::lowest());
    for (const auto& point : points) {
        target_bounds.expand_by(point);
    }

    return target_bounds;
}

Expected<radix::geometry::Aabb3d> encompassing_bounds_transfer(const OGRSpatialReference& source_srs,
    const OGRSpatialReference& target_srs,
    const radix::geometry::Aabb3d& source_bounds,
    const uint32_t intermediate_points_edges,
    const uint32_t intermediate_points_faces)
{
    if (source_srs.IsSame(&target_srs)) {
        return source_bounds;
    }

    auto transform = srs::transformation(source_srs, target_srs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    return encompassing_bounds_transfer(transform->get(), source_bounds, intermediate_points_edges, intermediate_points_faces);
}

std::expected<OGRSpatialReference, std::string> from_epsg(const uint32_t epsg)
{
    OGRSpatialReference srs;
    if (srs.importFromEPSG(epsg) != OGRERR_NONE) {
        return std::unexpected(fmt::format("Failed to import spatial reference from EPSG code: {}", epsg));
    }
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return srs;
}

std::expected<OGRSpatialReference, std::string> from_user_input(const std::string& user_input)
{
    OGRSpatialReference srs;
    if (srs.SetFromUserInput(user_input.c_str()) != OGRERR_NONE) {
        return std::unexpected(fmt::format("Failed to set spatial reference from user input: {}", user_input));
    }
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return srs;
}

namespace {
    // Points GDAL inserts between the corners of each edge when transforming bounds.
    constexpr int boundary_samples = 257;

    // Transforms bounds with GDAL, which samples the densified boundary. Extrema of curved
    // edges can lie between samples, so each side is widened by a full sampling interval.
    // Geographic bounds crossing the antimeridian are unwrapped, i.e., max.x exceeds 180.
    Expected<radix::tile::SrsBounds> guarded_bounds_transform(OGRCoordinateTransformation* transform, const radix::tile::SrsBounds& bounds)
    {
        radix::tile::SrsBounds result;
        if (!transform->TransformBounds(
                bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y, &result.min.x, &result.min.y, &result.max.x, &result.max.y, boundary_samples)) {
            return Error::fail(Error::Code::InvalidInput, "transform bounds");
        }
        if (!std::isfinite(result.min.x) || !std::isfinite(result.max.x) || !std::isfinite(result.min.y) || !std::isfinite(result.max.y)) {
            return Error::fail(Error::Code::InvalidInput, "nonfinite transformed bounds");
        }
        // TransformBounds reports antimeridian-crossing bounds with max.x < min.x.
        if (result.max.x < result.min.x) {
            result.max.x += 360;
        }
        const glm::dvec2 guard = glm::max(glm::dvec2(1e-6), result.size() / double(boundary_samples + 1));
        result.min -= guard;
        result.max += guard;
        return result;
    }

    // Splits longitude/latitude bounds into at most two bounds with longitudes in [-180, 180].
    // Bounds crossing the antimeridian may be given with max.x < min.x or max.x beyond 180.
    std::vector<radix::tile::SrsBounds> split_at_antimeridian(const radix::tile::SrsBounds& bounds)
    {
        double west = bounds.min.x;
        double east = bounds.max.x;
        const double south = bounds.min.y;
        const double north = bounds.max.y;
        if (east < west) {
            east += 360;
        }
        if (east - west >= 360) {
            return { { { -180, south }, { 180, north } } };
        }
        const double shift = 360 * std::floor((west + 180) / 360);
        west -= shift;
        east -= shift;
        if (east > 180) {
            return { { { west, south }, { 180, north } }, { { -180, south }, { east - 360, north } } };
        }
        return { { { west, south }, { east, north } } };
    }

    bool finite_and_ordered(const radix::geometry::Aabb3d& bounds)
    {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) || bounds.min[axis] > bounds.max[axis]) {
                return false;
            }
        }
        return true;
    }

    // Distance in metres by which horizontal datum shifts at the given ellipsoidal heights in metres
    // may differ from those at height zero, at which bounds are transformed. Empirical: a survey of
    // the PROJ 9.6 operations of 27 datums to and from WGS84 found at most 0.19 mm per metre of
    // height (Pulkovo 1942); this allows 1.6 times that.
    double datum_margin(double min_height, double max_height) { return 0.0003 * std::max(std::abs(min_height), std::abs(max_height)); }

    // Longitude ranges in degrees of the points in an ECEF xy rectangle. Longitude is the angle
    // of the point around the polar axis, so its range is spanned by the corners, unless the
    // rectangle contains the polar axis (all longitudes) or crosses the antimeridian (split).
    std::vector<glm::dvec2> longitude_ranges(const glm::dvec2& min, const glm::dvec2& max)
    {
        const auto corner_range = [](double x0, double x1, double y0, double y1) {
            glm::dvec2 range(std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest());
            for (const double x : { x0, x1 }) {
                for (const double y : { y0, y1 }) {
                    const double longitude = glm::degrees(std::atan2(y, x));
                    range = { std::min(range.x, longitude), std::max(range.y, longitude) };
                }
            }
            return range;
        };
        if (min.x <= 0 && max.x >= 0 && min.y <= 0 && max.y >= 0) {
            return { { -180, 180 } };
        }
        if (max.x < 0 && min.y < 0 && max.y >= 0) {
            // The antimeridian is the negative x half-axis; atan2 maps y = +0 to 180 and y = -0 to -180.
            return { corner_range(min.x, max.x, 0.0, max.y), corner_range(min.x, max.x, min.y, -0.0) };
        }
        return { corner_range(min.x, max.x, min.y, max.y) };
    }
} // namespace

Expected<std::vector<radix::tile::SrsBounds>> geographic_coverage(const OGRSpatialReference& reference, const radix::tile::SrsBounds& bounds)
{
    if ((bounds.min.x < -webmercator_half_extent || bounds.max.x > webmercator_half_extent) && webmercator().IsSame(&reference)) {
        return Error::fail(Error::Code::Unsupported, "coverage of Web Mercator bounds outside the world extent");
    }
    const auto geographic = wgs84();
    radix::tile::SrsBounds longitude_latitude = bounds;
    if (!reference.IsSame(&geographic)) {
        auto transform = transformation(reference, geographic);
        if (!transform) {
            return Error::propagate(std::move(transform));
        }
        auto transformed = guarded_bounds_transform(transform->get(), bounds);
        if (!transformed) {
            return Error::propagate(std::move(transformed), "transform bounds to geographic coordinates");
        }
        longitude_latitude = *transformed;
        longitude_latitude.min.y = std::max(-90.0, longitude_latitude.min.y);
        longitude_latitude.max.y = std::min(90.0, longitude_latitude.max.y);
    }
    if (!std::isfinite(longitude_latitude.min.x) || !std::isfinite(longitude_latitude.max.x) || !std::isfinite(longitude_latitude.min.y)
        || !std::isfinite(longitude_latitude.max.y)) {
        return Error::fail(Error::Code::InvalidInput, "nonfinite geographic coverage bounds");
    }
    return split_at_antimeridian(longitude_latitude);
}

Expected<std::vector<radix::tile::SrsBounds>> mercator_coverage(const OGRSpatialReference& reference, const radix::tile::SrsBounds& bounds)
{
    const auto mercator = webmercator();
    const glm::dvec2 half_extent(webmercator_half_extent);
    // Avoid the geographic round trip, it would widen exact Web Mercator bounds by a guard band.
    if (reference.IsSame(&mercator) && bounds.min.x >= -webmercator_half_extent && bounds.max.x <= webmercator_half_extent) {
        const radix::tile::SrsBounds clipped { glm::max(bounds.min, -half_extent), glm::min(bounds.max, half_extent) };
        if (clipped.min.x >= clipped.max.x || clipped.min.y >= clipped.max.y) {
            return std::vector<radix::tile::SrsBounds> {};
        }
        return std::vector<radix::tile::SrsBounds> { clipped };
    }
    auto geographic = geographic_coverage(reference, bounds);
    if (!geographic) {
        return geographic;
    }
    std::vector<radix::tile::SrsBounds> result;
    for (const auto& longitude_latitude : *geographic) {
        // Web Mercator diverges towards the poles; PROJ returns large finite values there.
        const double south = std::max(-webmercator_latitude_limit, longitude_latitude.min.y);
        const double north = std::min(webmercator_latitude_limit, longitude_latitude.max.y);
        if (south >= north) {
            continue;
        }
        auto corners
            = transform_points(wgs84(), mercator, std::array { glm::dvec2(longitude_latitude.min.x, south), glm::dvec2(longitude_latitude.max.x, north) });
        if (!corners) {
            return Error::propagate(std::move(corners));
        }
        result.push_back({ glm::clamp((*corners)[0], -half_extent, half_extent), glm::clamp((*corners)[1], -half_extent, half_extent) });
    }
    return result;
}

Expected<radix::geometry::Aabb3d> ecef_coverage(const OGRSpatialReference& reference, const radix::geometry::Aabb3d& bounds)
{
    if (!finite_and_ordered(bounds)) {
        return Error::fail(Error::Code::InvalidInput, "ECEF coverage requires finite, ordered bounds");
    }
    const auto geocentric = ecef();
    if (reference.IsSame(&geocentric)) {
        return bounds;
    }
    if (reference.IsCompound()) {
        return Error::fail(Error::Code::Unsupported, "ECEF coverage of compound SRS " + friendly_name(reference));
    }
    if (bounds.min.z < -20'000 || bounds.max.z > 200'000) {
        return Error::fail(Error::Code::Unsupported, "ECEF coverage requires heights between -20000 and 200000 metres");
    }
    auto geographic = geographic_coverage(reference, radix::tile::SrsBounds(bounds));
    if (!geographic) {
        return Error::propagate(std::move(geographic));
    }
    // x = (N + h) cos(latitude) cos(longitude), y = (N + h) cos(latitude) sin(longitude) and
    // z = (N (1 - e^2) + h) sin(latitude). x turns at longitudes 0 and 180, y at -90 and 90,
    // both at the equator, z nowhere. Splitting a box at the turning values inside it gives
    // boxes whose extrema are at their corners, i.e., at combinations of these values.
    std::vector<glm::dvec3> candidates;
    for (const auto& box : *geographic) {
        std::vector<double> longitudes { box.min.x, box.max.x };
        for (const double turn : { -90.0, 0.0, 90.0 }) {
            if (box.min.x < turn && turn < box.max.x) {
                longitudes.push_back(turn);
            }
        }
        std::vector<double> latitudes { box.min.y, box.max.y };
        if (box.min.y < 0 && 0 < box.max.y) {
            latitudes.push_back(0);
        }
        for (const double longitude : longitudes) {
            for (const double latitude : latitudes) {
                for (const double height : { bounds.min.z, bounds.max.z }) {
                    candidates.emplace_back(longitude, latitude, height);
                }
            }
        }
    }
    auto points = transform_points(wgs84(), geocentric, std::move(candidates));
    if (!points) {
        return Error::propagate(std::move(points), "transform coverage candidates to ECEF");
    }
    radix::geometry::Aabb3d result;
    for (const auto& point : *points) {
        result.expand_by(point);
    }
    // Allow for rounding in the transformation, and for datum shifts at the heights of the bounds.
    double margin = 1e-3;
    if (!wgs84().IsSame(&reference) && !webmercator().IsSame(&reference)) {
        margin += datum_margin(bounds.min.z, bounds.max.z);
    }
    result.min -= glm::dvec3(margin);
    result.max += glm::dvec3(margin);
    return result;
}

Expected<std::vector<radix::geometry::Aabb3d>> ecef2srs_coverage(const radix::geometry::Aabb3d& bounds, const OGRSpatialReference& reference)
{
    if (!finite_and_ordered(bounds)) {
        return Error::fail(Error::Code::InvalidInput, "coverage of ECEF bounds requires finite, ordered bounds");
    }
    const auto geocentric = ecef();
    if (reference.IsSame(&geocentric)) {
        return std::vector<radix::geometry::Aabb3d> { bounds };
    }
    if (reference.IsCompound()) {
        return Error::fail(Error::Code::Unsupported, "coverage of ECEF bounds in compound SRS " + friendly_name(reference));
    }

    // Latitude and height depend only on z and the distance p to the polar axis; the box covers
    // [p_min, p_max] x [z_min, z_max]. Latitude is monotonic in p and z, height in p, and in z
    // within each hemisphere. The extrema are at the corners, the height minimum possibly at z = 0.
    // This breaks within the evolute of the meridian ellipse around the centre, which extends
    // 42.7 km from the polar axis and 42.8 km from the equatorial plane; PROJ is erratic there.
    // Elsewhere, PROJ's inverse is approximate deep inside the earth (metres at 1000 km radius).
    const glm::dvec2 closest = glm::clamp(glm::dvec2(0), glm::dvec2(bounds.min), glm::dvec2(bounds.max));
    constexpr double evolute_extent = 43000;
    if (glm::length(closest) < evolute_extent && bounds.min.z < evolute_extent && bounds.max.z > -evolute_extent) {
        return Error::fail(Error::Code::Unsupported, "coverage of ECEF bounds near the centre of the earth");
    }
    double p_max = 0;
    for (const double x : { bounds.min.x, bounds.max.x }) {
        for (const double y : { bounds.min.y, bounds.max.y }) {
            p_max = std::max(p_max, std::hypot(x, y));
        }
    }
    std::vector<double> heights { bounds.min.z, bounds.max.z };
    if (bounds.min.z < 0 && 0 < bounds.max.z) {
        heights.push_back(0);
    }
    std::vector<glm::dvec3> candidates;
    for (const double p : { glm::length(closest), p_max }) {
        for (const double z : heights) {
            candidates.emplace_back(p, 0, z);
        }
    }
    auto points = transform_points(geocentric, wgs84(), std::move(candidates));
    if (!points) {
        return Error::propagate(std::move(points), "transform coverage candidates to geographic coordinates");
    }
    glm::dvec2 latitude_range(std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest());
    glm::dvec2 height_range = latitude_range;
    for (const auto& point : *points) {
        latitude_range = { std::min(latitude_range.x, point.y), std::max(latitude_range.y, point.y) };
        height_range = { std::min(height_range.x, point.z), std::max(height_range.y, point.z) };
    }
    // Allow for rounding in the transformation.
    latitude_range = glm::clamp(latitude_range + glm::dvec2(-1e-9, 1e-9), -90.0, 90.0);
    height_range += glm::dvec2(-1e-3, 1e-3);

    std::vector<radix::tile::SrsBounds> geographic;
    for (const auto& longitude_range : longitude_ranges(glm::dvec2(bounds.min), glm::dvec2(bounds.max))) {
        geographic.push_back(
            { { std::max(-180.0, longitude_range.x - 1e-9), latitude_range.x }, { std::min(180.0, longitude_range.y + 1e-9), latitude_range.y } });
    }

    // Web Mercator x depends only on longitude and y only on latitude, so its bounds follow exactly.
    // Other SRSes get GDAL's densified boundary transformation with a guard band.
    std::vector<radix::tile::SrsBounds> covered;
    const auto wgs84_srs = wgs84();
    const auto webmercator_srs = webmercator();
    if (reference.IsSame(&wgs84_srs)) {
        covered = std::move(geographic);
    } else if (reference.IsSame(&webmercator_srs)) {
        for (const auto& box : geographic) {
            auto mercator = mercator_coverage(wgs84_srs, box);
            if (!mercator) {
                return Error::propagate(std::move(mercator));
            }
            covered.insert(covered.end(), mercator->begin(), mercator->end());
        }
    } else {
        // Allow for datum shifts at the heights of the box: the margin is a distance, so the
        // geographic bounds of the expanded box include all points within it.
        const double margin = datum_margin(height_range.x, height_range.y);
        auto expanded = ecef2srs_coverage({ bounds.min - margin, bounds.max + margin }, wgs84_srs);
        if (!expanded) {
            return Error::propagate(std::move(expanded));
        }
        auto transform = transformation(wgs84_srs, reference);
        if (!transform) {
            return Error::propagate(std::move(transform));
        }
        for (const auto& box : *expanded) {
            auto transformed = guarded_bounds_transform(transform->get(), radix::tile::SrsBounds(box));
            if (!transformed) {
                return Error::propagate(std::move(transformed), "transform coverage to " + friendly_name(reference));
            }
            if (!reference.IsGeographic()) {
                covered.push_back(*transformed);
                continue;
            }
            transformed->min.y = std::max(-90.0, transformed->min.y);
            transformed->max.y = std::min(90.0, transformed->max.y);
            const auto split = split_at_antimeridian(*transformed);
            covered.insert(covered.end(), split.begin(), split.end());
        }
    }

    std::vector<radix::geometry::Aabb3d> result;
    for (const auto& box : covered) {
        result.push_back({ { box.min, height_range.x }, { box.max, height_range.y } });
    }
    return result;
}

} // namespace srs
