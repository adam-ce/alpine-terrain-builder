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

namespace srs {

std::optional<int> epsg_code(const OGRSpatialReference& srs);

std::string friendly_name(const OGRSpatialReference& srs);

std::unique_ptr<OGRSpatialReference> clone(const OGRSpatialReference& srs);

std::unique_ptr<OGRCoordinateTransformation> uncached_transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs);

std::shared_ptr<OGRCoordinateTransformation> transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs);

template <typename T>
inline glm::tvec2<T> transform_point(OGRCoordinateTransformation* transform, glm::tvec2<T> p)
{
    if (!transform->Transform(1, &p.x, &p.y))
        throw std::runtime_error("srs::transform_point(glm::tvec2<T>) failed");
    return p;
}
template <typename T>
inline glm::tvec3<T> transform_point(OGRCoordinateTransformation* transform, glm::tvec3<T> p)
{
    if (!transform->Transform(1, &p.x, &p.y, &p.z))
        throw std::runtime_error("srs::transform_point(glm::tvec3<T>) failed");
    return p;
}

template <typename T>
inline glm::tvec2<T> transform_point(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, glm::tvec2<T> p)
{
    const auto transform = transformation(source_srs, target_srs);
    return transform_point(transform.get(), p);
}
template <typename T>
inline glm::tvec3<T> transform_point(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, glm::tvec3<T> p)
{
    const auto transform = transformation(source_srs, target_srs);
    return transform_point(transform.get(), p);
}

template <typename Container>
inline void transform_points_inplace(OGRCoordinateTransformation* transform, Container& points)
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
        throw std::runtime_error("srs::transform_points_inplace failed");
    }

    for (size_t i = 0; i < size; i++) {
        if constexpr (is_3d) {
            points[i] = PointType(xs[i], ys[i], zs[i]);
        } else {
            points[i] = PointType(xs[i], ys[i]);
        }
    }
}

template <typename Container>
inline Container transform_points(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, Container points)
{
    const auto transform = transformation(source_srs, target_srs);
    transform_points_inplace(transform.get(), points);
    return points;
}

// TODO: somehow integrate into a single transform_points
template <typename T>
inline std::vector<glm::tvec2<T>> transform_points_to_2d(OGRCoordinateTransformation* transform, const std::vector<glm::tvec3<T>>& points)
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
        throw std::runtime_error("srs::transform_points_to_2d(OGRCoordinateTransformation *, std::vector<glm::tvec3<T>>) failed");
    }

    std::vector<glm::tvec2<T>> transformed;
    transformed.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        transformed[i] = { xs[i], ys[i] };
    }

    return transformed;
}

radix::tile::SrsBounds non_exact_bounds_transform(OGRCoordinateTransformation* transform, const radix::tile::SrsBounds& bounds);
radix::tile::SrsBounds non_exact_bounds_transform(
    const radix::tile::SrsBounds& bounds, const OGRSpatialReference& sourceSrs, const OGRSpatialReference& targetSrs);
radix::geometry::Aabb3d non_exact_bounds_transform(OGRCoordinateTransformation* transform, const radix::geometry::Aabb3d& bounds);
radix::geometry::Aabb3d non_exact_bounds_transform(
    const radix::geometry::Aabb3d& bounds, const OGRSpatialReference& sourceSrs, const OGRSpatialReference& targetSrs);

/// Transforms bounds from one srs to another,
/// in such a way that all points inside the original bounds are guaranteed to also be in the new bounds.
/// But there can be points inside the new bounds that were not present in the original ones.
radix::tile::SrsBounds encompassing_bounds_transfer(OGRCoordinateTransformation* transform, const radix::tile::SrsBounds& source_bounds);
radix::tile::SrsBounds encompassing_bounds_transfer(
    const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs, const radix::tile::SrsBounds& source_bounds);

radix::geometry::Aabb3d encompassing_bounds_transfer(OGRCoordinateTransformation* transform,
    const radix::geometry::Aabb3d& source_bounds,
    const uint32_t intermediate_points_edges = 21,
    const uint32_t intermediate_points_faces = 5);
radix::geometry::Aabb3d encompassing_bounds_transfer(const OGRSpatialReference& source_srs,
    const OGRSpatialReference& target_srs,
    const radix::geometry::Aabb3d& source_bounds,
    const uint32_t intermediate_points_edges = 21,
    const uint32_t intermediate_points_faces = 5);

std::expected<OGRSpatialReference, std::string> from_epsg(const uint32_t epsg);

std::expected<OGRSpatialReference, std::string> from_user_input(const std::string& user_input);

inline OGRSpatialReference ecef() { return from_epsg(4978).value(); }
inline OGRSpatialReference webmercator() { return from_epsg(3857).value(); }
inline OGRSpatialReference wgs84() { return from_epsg(4326).value(); }
inline OGRSpatialReference mgi() { return from_epsg(4312).value(); }

} // namespace srs
