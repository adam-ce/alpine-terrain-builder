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

Expected<std::vector<radix::tile::SrsBounds>> geodetic_coverage(const OGRSpatialReference& reference, const radix::tile::SrsBounds& bounds)
{
    constexpr int boundary_samples = 257;
    const auto geodetic = wgs84();
    radix::tile::SrsBounds longitude_latitude = bounds;
    const bool reprojected = !reference.IsSame(&geodetic);
    if (reprojected) {
        auto transform = transformation(reference, geodetic);
        if (!transform) {
            return Error::propagate(std::move(transform));
        }
        if (!(*transform)
                ->TransformBounds(bounds.min.x,
                    bounds.min.y,
                    bounds.max.x,
                    bounds.max.y,
                    &longitude_latitude.min.x,
                    &longitude_latitude.min.y,
                    &longitude_latitude.max.x,
                    &longitude_latitude.max.y,
                    boundary_samples)) {
            return Error::fail(Error::Code::InvalidInput, "transform bounds to geodetic coordinates");
        }
    }
    double west = longitude_latitude.min.x;
    double east = longitude_latitude.max.x;
    double south = longitude_latitude.min.y;
    double north = longitude_latitude.max.y;
    if (!std::isfinite(west) || !std::isfinite(east) || !std::isfinite(south) || !std::isfinite(north)) {
        return Error::fail(Error::Code::InvalidInput, "nonfinite geodetic coverage bounds");
    }
    // TransformBounds reports antimeridian-crossing bounds with east < west.
    if (east < west) {
        east += 360;
    }
    if (reprojected) {
        // Densified transformation samples are not exact extrema of curved edges.
        // Retain a full sampling interval as a guard band rather than pruning at
        // the sampled extremum.
        const double longitude_guard = std::max(1e-6, (east - west) / (boundary_samples + 1));
        const double latitude_guard = std::max(1e-6, (north - south) / (boundary_samples + 1));
        west -= longitude_guard;
        east += longitude_guard;
        south = std::max(-90.0, south - latitude_guard);
        north = std::min(90.0, north + latitude_guard);
    }
    if (east - west >= 360) {
        return std::vector<radix::tile::SrsBounds> { { { -180, south }, { 180, north } } };
    }
    const double shift = 360 * std::floor((west + 180) / 360);
    west -= shift;
    east -= shift;
    if (east > 180) {
        return std::vector<radix::tile::SrsBounds> { { { west, south }, { 180, north } }, { { -180, south }, { east - 360, north } } };
    }
    return std::vector<radix::tile::SrsBounds> { { { west, south }, { east, north } } };
}

Expected<std::vector<radix::tile::SrsBounds>> mercator_coverage(const OGRSpatialReference& reference, const radix::tile::SrsBounds& bounds)
{
    const auto mercator = webmercator();
    const glm::dvec2 half_extent(webmercator_half_extent);
    // Avoid the geodetic round trip, it would widen exact Web Mercator bounds by a guard band.
    if (reference.IsSame(&mercator) && bounds.min.x >= -webmercator_half_extent && bounds.max.x <= webmercator_half_extent) {
        const radix::tile::SrsBounds clipped { glm::max(bounds.min, -half_extent), glm::min(bounds.max, half_extent) };
        if (clipped.min.x >= clipped.max.x || clipped.min.y >= clipped.max.y) {
            return std::vector<radix::tile::SrsBounds> {};
        }
        return std::vector<radix::tile::SrsBounds> { clipped };
    }
    auto geodetic = geodetic_coverage(reference, bounds);
    if (!geodetic) {
        return geodetic;
    }
    std::vector<radix::tile::SrsBounds> result;
    for (const auto& longitude_latitude : *geodetic) {
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

} // namespace srs
