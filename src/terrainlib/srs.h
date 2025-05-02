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

#ifndef SRS_H
#define SRS_H

#include <cstddef>
#include <glm/detail/qualifier.hpp>
#include <memory>

#include <glm/glm.hpp>
#include <ogr_spatialref.h>
#include <vector>

#include "Exception.h"
#include "tntn/geometrix.h"
#include <radix/tile.h>

namespace srs {

inline std::unique_ptr<OGRCoordinateTransformation> transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs) {
    auto transformer = std::unique_ptr<OGRCoordinateTransformation>(OGRCreateCoordinateTransformation(&source_srs, &target_srs));
    if (!transformer) {
        throw Exception("Couldn't create SRS transformation");
    }
    return transformer;
}

template <typename T>
inline glm::tvec2<T> transform_point(OGRCoordinateTransformation *transform, glm::tvec2<T> p) {
    if (!transform->Transform(1, &p.x, &p.y))
        throw Exception("srs::transform_point(glm::tvec2<T>) failed");
    return p;
}
template <typename T>
inline glm::tvec3<T> transform_point(OGRCoordinateTransformation *transform, glm::tvec3<T> p) {
    if (!transform->Transform(1, &p.x, &p.y, &p.z))
        throw Exception("srs::transform_point(glm::tvec3<T>) failed");
    return p;
}

template <typename T>
inline glm::tvec2<T> transform_point(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, glm::tvec2<T> p) {
    const auto transform = transformation(source_srs, target_srs);
    return transform_point(transform.get(), p);
}
template <typename T>
inline glm::tvec3<T> transform_point(const OGRSpatialReference &source_srs, const OGRSpatialReference& target_srs, glm::tvec3<T> p){
    const auto transform = transformation(source_srs, target_srs);
    return transform_point(transform.get(), p);
}

template <typename T, std::size_t n>
inline void transform_points_inplace(OGRCoordinateTransformation *transform, std::array<glm::tvec2<T>, n> &points) {
    std::array<T, n> xs;
    std::array<T, n> ys;

    for (size_t i = 0; i < points.size(); i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
    }

    if (!transform->Transform(points.size(), xs.data(), ys.data())) {
        throw Exception("srs::transform_points_inplace(std::array<glm::tvec2<T>, n>) failed");
    }

    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = {xs[i], ys[i]};
    }
}
template <typename T>
inline void transform_points_inplace(OGRCoordinateTransformation *transform, std::vector<glm::tvec2<T>> &points) {
    std::vector<T> xs;
    std::vector<T> ys;

    for (size_t i = 0; i < points.size(); i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
    }

    if (!transform->Transform(points.size(), xs.data(), ys.data())) {
        throw Exception("srs::transform_points_inplace(std::vector<glm::tvec2<T>, n>) failed");
    }

    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = {xs[i], ys[i]};
    }
}

template <typename T, std::size_t n>
inline void transform_points_inplace(OGRCoordinateTransformation *transform, std::array<glm::tvec3<T>, n> &points) {
    std::array<T, n> xs;
    std::array<T, n> ys;
    std::array<T, n> zs;

    for (size_t i = 0; i < points.size(); i++) {
        xs[i] = points[i].x;
        ys[i] = points[i].y;
        zs[i] = points[i].z;
    }

    if (!transform->Transform(points.size(), xs.data(), ys.data(), zs.data())) {
        throw Exception("srs::transform_points_inplace(std::array<glm::tvec3<T>, n>) failed");
    }

    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = {xs[i], ys[i], zs[i]};
    }
}
template <typename T>
inline void transform_points_inplace(OGRCoordinateTransformation *transform, std::vector<glm::tvec3<T>> &points) {
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
        throw Exception("srs::transform_points_inplace(std::vector<glm::tvec3<T>, n>) failed");
    }

    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = {xs[i], ys[i], zs[i]};
    }
}

template <typename Container>
inline Container transform_points(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, Container points) {
    const auto transform = transformation(source_srs, target_srs);
    transform_points_inplace(transform.get(), points);
    return points;
}


// TODO: somehow integrate into a single transform_points
template <typename T>
inline std::vector<glm::tvec2<T>> transform_points_to_2d(OGRCoordinateTransformation *transform, const std::vector<glm::tvec3<T>> &points) {
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
        throw Exception("srs::transform_points_inplace(std::vector<glm::tvec3<T>, n>) failed");
    }

    std::vector<glm::tvec2<T>> transformed;
    transformed.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        transformed[i] = {xs[i], ys[i]};
    }

    return transformed;
}

inline radix::tile::SrsBounds non_exact_bounds_transform(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &sourceSrs, const OGRSpatialReference &targetSrs) {
    const auto transform = transformation(sourceSrs, targetSrs);
    std::array xs = {bounds.min.x, bounds.max.x};
    std::array ys = {bounds.min.y, bounds.max.y};
    if (!transform->Transform(2, xs.data(), ys.data())) {
        throw Exception("srs::non_exact_bounds_transform failed");
    }
    return {{xs[0], ys[0]}, {xs[1], ys[1]}};
}

inline radix::geometry::Aabb3d non_exact_bounds_transform(const radix::geometry::Aabb3d &bounds, const OGRSpatialReference &sourceSrs, const OGRSpatialReference &targetSrs) {
    const auto transform = transformation(sourceSrs, targetSrs);
    std::array xs = {bounds.min.x, bounds.max.x};
    std::array ys = {bounds.min.y, bounds.max.y};
    std::array zs = {bounds.min.z, bounds.max.z};
    if (!transform->Transform(2, xs.data(), ys.data(), zs.data())) {
        throw Exception("srs::non_exact_bounds_transform failed");
    }
    return {{xs[0], ys[0], zs[0]}, {xs[1], ys[1], zs[1]}};
}

/// Transforms bounds from one srs to another,
/// in such a way that all points inside the original bounds are guaranteed to also be in the new bounds.
/// But there can be points inside the new bounds that were not present in the original ones.
inline radix::tile::SrsBounds encompassing_bounds_transfer(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, const radix::tile::SrsBounds &source_bounds) {
    if (source_srs.IsSame(&target_srs)) {
        return source_bounds;
    }

    const std::unique_ptr<OGRCoordinateTransformation> transformation = srs::transformation(source_srs, target_srs);
    radix::tile::SrsBounds target_bounds;
    const int result = transformation->TransformBounds(
        source_bounds.min.x, source_bounds.min.y, source_bounds.max.x, source_bounds.max.y,
        &target_bounds.min.x, &target_bounds.min.y, &target_bounds.max.x, &target_bounds.max.y,
        21);
    if (result != TRUE) {
        throw std::runtime_error("srs::encompassing_bounding_box_transfer failed");
    }
    return target_bounds;
}

inline radix::geometry::Aabb3d encompassing_bounds_transfer(
    const OGRSpatialReference &source_srs,
    const OGRSpatialReference &target_srs,
    const radix::geometry::Aabb3d &source_bounds,
    const uint32_t intermediate_points_edges = 21,
    const uint32_t intermediate_points_faces = 5) {
    if (source_srs.IsSame(&target_srs)) {
        return source_bounds;
    }

    std::vector<glm::dvec3> points;

    // Add corner points
    const auto corners = radix::geometry::corners(source_bounds);
    std::copy(corners.begin(), corners.end(), std::back_inserter(points));

    // Sample points on the edges
    const auto edges = radix::geometry::edges(source_bounds);
    for (const auto &edge : edges) {
        const auto &[p0, p1] = edge;

        for (uint32_t i = 1; i <= intermediate_points_edges; i++) {
            const double t = static_cast<double>(i) / (intermediate_points_edges + 1);
            const auto p = glm::mix(p0, p1, t);
            points.push_back(p);
        }
    }

    // TODO: do we need this?
    // Sample points on the faces
    const auto quads = radix::geometry::quads(source_bounds);
    for (const auto &quad : quads) {
        const auto &[p0, p1, p2, p3] = quad;

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
    const auto transform = srs::transformation(source_srs, target_srs);
    transform_points_inplace(transform.get(), points);

    // Compute bounds from transformed points
    radix::geometry::Aabb3d target_bounds;
    target_bounds.min = glm::dvec3(std::numeric_limits<double>::max());
    target_bounds.max = glm::dvec3(std::numeric_limits<double>::min());
    for (const auto &point : points) {
        target_bounds.expand_by(point);
    }

    return target_bounds;
}

inline OGRSpatialReference from_epsg(uint32_t epsg) {
    OGRSpatialReference srs;
    srs.importFromEPSG(epsg);
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return srs;
}
inline OGRSpatialReference ecef() {
    return from_epsg(4978);
}
inline OGRSpatialReference webmercator() {
    return from_epsg(3857);
}
inline OGRSpatialReference wgs84() {
    return from_epsg(4326);
}
inline OGRSpatialReference mgi() {
    return from_epsg(4312);
}

// only for tntn
template <typename T>
inline glm::tvec3<T> toECEF(const OGRSpatialReference& source_srs, const glm::tvec3<T>& p) {
    return transform_point(source_srs, ecef(), p);
}

template <typename T>
inline std::vector<glm::tvec3<T>> toECEF(const OGRSpatialReference& source_srs, std::vector<glm::tvec3<T>> points) {
    return transform_points(source_srs, ecef(), points);
}

template <typename T, std::size_t n>
inline std::array<glm::tvec3<T>, n> toECEF(const OGRSpatialReference& source_srs, std::array<glm::tvec3<T>, n> points) {
    return transform_points(source_srs, ecef(), points);
}

template <typename T>
inline std::array<glm::tvec3<T>, 2> toECEF(const OGRSpatialReference& source_srs, const glm::tvec3<T>& p1, const glm::tvec3<T>& p2) {
    return toECEF<T, 2>(source_srs, { p1, p2 });
}
inline tntn::BBox3D toECEF(const OGRSpatialReference& source_srs, const tntn::BBox3D& box) {
    constexpr auto n_samples = 100;
    std::vector<glm::dvec3> points;
    points.emplace_back(box.min.x, box.min.y, box.min.z);
    points.emplace_back(box.min.x, box.min.y, box.max.z);
    points.emplace_back(box.min.x, box.max.y, box.min.z);
    points.emplace_back(box.min.x, box.max.y, box.max.z);
    points.emplace_back(box.max.x, box.min.y, box.min.z);
    points.emplace_back(box.max.x, box.min.y, box.max.z);
    points.emplace_back(box.max.x, box.max.y, box.min.z);
    points.emplace_back(box.max.x, box.max.y, box.max.z);

    const auto dx = (box.max.x - box.min.x) / n_samples;
    const auto dy = (box.max.y - box.min.y) / n_samples;
    for (auto i = 0; i < n_samples; ++i) {
        // top and bottom
        points.emplace_back(box.min.x + i * dx, box.min.y, box.min.z);
        points.emplace_back(box.min.x + i * dx, box.min.y, box.max.z);
        points.emplace_back(box.min.x + i * dx, box.max.y, box.min.z);
        points.emplace_back(box.min.x + i * dx, box.max.y, box.max.z);
        // left and right
        points.emplace_back(box.min.x, box.min.y + i * dy, box.min.z);
        points.emplace_back(box.min.x, box.min.y + i * dy, box.max.z);
        points.emplace_back(box.max.x, box.min.y + i * dy, box.min.z);
        points.emplace_back(box.max.x, box.min.y + i * dy, box.max.z);
    }
    const auto ecef_points = toECEF(source_srs, points);
    tntn::BBox3D resulting_bbox;
    resulting_bbox.add(ecef_points.begin(), ecef_points.end());
    return resulting_bbox;
}

}

#endif // SRS_H
