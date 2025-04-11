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

inline std::unique_ptr<OGRCoordinateTransformation> transformation(const OGRSpatialReference& source, const OGRSpatialReference& targetSrs)
{
    const auto data_srs = source;
    auto transformer = std::unique_ptr<OGRCoordinateTransformation>(OGRCreateCoordinateTransformation(&data_srs, &targetSrs));
    if (!transformer)
        throw Exception("Couldn't create SRS transformation");
    return transformer;
}

// this transform is non exact, because we are only transforming the corner vertices. however, due to projection warping, a rectangle can become an trapezoid with curved edges.
inline radix::tile::SrsBounds nonExactBoundsTransform(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &sourceSrs, const OGRSpatialReference &targetSrs) {
    const auto transform = transformation(sourceSrs, targetSrs);
    std::array xes = { bounds.min.x, bounds.max.x };
    std::array yes = { bounds.min.y, bounds.max.y };
    if (!transform->Transform(2, xes.data(), yes.data()))
        throw Exception("nonExactBoundsTransform failed");
    return { {xes[0], yes[0]}, {xes[1], yes[1]} };
}

template <typename T>
inline glm::tvec3<T> to(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, glm::tvec3<T> p)
{
    const auto transform = transformation(source_srs, target_srs);
    if (!transform->Transform(1, &p.x, &p.y, &p.z))
        throw Exception("srs::to(glm::tvec3<T>) failed");
    return p;
}

template <typename T>
inline glm::tvec2<T> to(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, glm::tvec2<T> p) {
    const auto transform = transformation(source_srs, target_srs);
    if (!transform->Transform(1, &p.x, &p.y))
        throw Exception("srs::to(glm::tvec2<T>) failed");
    return p;
}

template <typename T>
inline glm::tvec3<T> toECEF(const OGRSpatialReference& source_srs, const glm::tvec3<T>& p)
{
    OGRSpatialReference ecef_srs;
    ecef_srs.importFromEPSG(4978);
    ecef_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return to(source_srs, ecef_srs, p);
}

template <typename T>
inline std::vector<glm::tvec3<T>> toECEF(const OGRSpatialReference& source_srs, std::vector<glm::tvec3<T>> points)
{
    std::vector<T> xes;
    std::vector<T> ys;
    std::vector<T> zs;
    xes.reserve(points.size());
    ys.reserve(points.size());
    zs.reserve(points.size());

    for (const auto& p : points) {
        xes.push_back(p.x);
        ys.push_back(p.y);
        zs.push_back(p.z);
    }

    OGRSpatialReference ecef_srs;
    ecef_srs.importFromEPSG(4978);
    ecef_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    const auto transform = transformation(source_srs, ecef_srs);
    if (!transform->Transform(int(points.size()), xes.data(), ys.data(), zs.data()))
        throw Exception("toECEF(glm::tvec3<T>) failed");

    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = { xes[i], ys[i], zs[i] };
    }
    return points;
}

template <typename T, std::size_t n>
inline std::array<glm::tvec3<T>, n> toECEF(const OGRSpatialReference& source_srs, std::array<glm::tvec3<T>, n> points)
{
    std::array<T, n> xes;
    std::array<T, n> ys;
    std::array<T, n> zs;

    for (size_t i = 0; i < points.size(); ++i) {
        xes[i] = points[i].x;
        ys[i] = points[i].y;
        zs[i] = points[i].z;
    }

    OGRSpatialReference ecef_srs;
    ecef_srs.importFromEPSG(4978);
    ecef_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    const auto transform = transformation(source_srs, ecef_srs);
    if (!transform->Transform(points.size(), xes.data(), ys.data(), zs.data()))
        throw Exception("toECEF(glm::tvec3<T>) failed");

    for (size_t i = 0; i < points.size(); ++i) {
        points[i] = { xes[i], ys[i], zs[i] };
    }
    return points;
}

template <typename T>
inline std::array<glm::tvec3<T>, 2> toECEF(const OGRSpatialReference& source_srs, const glm::tvec3<T>& p1, const glm::tvec3<T>& p2)
{
    return toECEF<T, 2>(source_srs, { p1, p2 });
}
inline tntn::BBox3D toECEF(const OGRSpatialReference& source_srs, const tntn::BBox3D& box)
{
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

/// Transforms bounds from one srs to another,
/// in such a way that all points inside the original bounds are guaranteed to also be in the new bounds.
/// But there can be points inside the new bounds that were not present in the original ones.
inline radix::tile::SrsBounds encompassing_bounding_box_transfer(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, const radix::tile::SrsBounds &source_bounds) {
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
        throw std::runtime_error("encompassing_bounding_box_transfer failed");
    }
    return target_bounds;
}
}

#endif // SRS_H
