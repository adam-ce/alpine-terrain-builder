/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2021 Adam Celarek-Litofcenko
 * Copyright (C) 2021 Martin Braunsperger
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

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>
#include <utility>
#include <vector>

#include <gdal.h>
#include <gdal_priv.h>
#include <glm/glm.hpp>
#include <radix/geometry.h>
#include <glm/gtx/norm.hpp>
#include <libassert/assert.hpp>

#include "Dataset.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "mesh/cleanup.h"
#include "mesh_builder.h"
#include <radix/raster.h>
#include "raw_dataset_reader.h"
#include "srs.h"
#include "mesh/clip.h"
#include "mesh/validate.h"
#include "mesh/bounds.h"

// TODO: fix namespace
namespace terrainbuilder {

std::ostream &operator<<(std::ostream &os, BuildMeshError error) {
    switch (error) {
    case BuildMeshError::OutOfBounds:
        os << "out of bounds";
        break;
    case BuildMeshError::EmptyRegion:
        os << "empty region";
        break;
    case BuildMeshError::TransformationFailed:
        os << "transformation failed";
        break;
    default:
        os << "unknown build error";
        break;
    }
    return os;
}

namespace {
using PixelBounds = radix::geometry::Aabb2i;
    
template <typename T>
[[maybe_unused]]
glm::dvec2 apply_transform(std::array<double, 6> transform, const glm::tvec2<T> &v) {
    glm::dvec2 result;
    GDALApplyGeoTransform(transform.data(), v.x, v.y, &result.x, &result.y);
    return result;
}

glm::dvec3 convert_pixel_to_vertex(const float height, const glm::uvec2 pixel_coords, const RawDatasetReader& reader, const PixelBounds& pixel_bounds) {
    const glm::dvec2 point_offset_in_raster(0.5); // Convert pixel coordinates into a point in the dataset's srs.
    const glm::dvec2 coords_raster_relative = glm::dvec2(pixel_coords) + point_offset_in_raster;
    const glm::dvec2 coords_raster_absolute = coords_raster_relative + glm::dvec2(pixel_bounds.min);
    const glm::dvec3 coords_source(reader.transform_pixel_to_srs_point(coords_raster_absolute), height);
    return coords_source;
}

SimpleMesh meshify(const radix::Raster<glm::dvec3>& source_points, const radix::RasterMask& mask) {
    // Compact the vertex grid into a list of valid ones.
    const size_t valid_vertex_count = std::reduce(mask.begin(), mask.end(), size_t(0));
    // Check if we even have any valid vertices. Can happen if all of the region is padding.
    if (valid_vertex_count == 0) {
        return SimpleMesh();
    }

    std::vector<glm::dvec3> positions;
    positions.reserve(valid_vertex_count);

    auto vertex_index_map_result = radix::raster::transform(source_points, mask, [&](const glm::dvec3& point) -> size_t {
        const size_t index = positions.size();
        positions.push_back(point);
        return index;
    });
    DEBUG_ASSERT(vertex_index_map_result.has_value());
    if (!vertex_index_map_result.has_value())
        return {};
    const auto vertex_index_map = std::move(*vertex_index_map_result);
    DEBUG_ASSERT(positions.size() == valid_vertex_count);

    // Allocate triangle vector
    const size_t max_triangle_count = (source_points.width() - 1) * (source_points.height() - 1) * 2;
    std::vector<glm::uvec3> triangles;
    triangles.reserve(max_triangle_count);

    for (unsigned y = 0; y < source_points.height() - 1; y++) {
        for (unsigned x = 0; x < source_points.width() - 1; x++) {
            const std::array<glm::uvec2, 4> quad {
                glm::uvec2{x, y},
                glm::uvec2{x + 1, y},
                glm::uvec2{x + 1, y + 1},
                glm::uvec2{x, y + 1}};

            for (uint32_t i = 0; i < 4; i++) {
                const auto& v0 = quad[i];
                const auto& v1 = quad[(i + 1) % 4];
                const auto& v2 = quad[(i + 2) % 4];

                // Check if the indices are valid
                if (mask.pixel(v0) && mask.pixel(v1) && mask.pixel(v2)) {
                    triangles.emplace_back(vertex_index_map.pixel(v0), vertex_index_map.pixel(v1), vertex_index_map.pixel(v2));
                    i++;
                }
            }
        }
    }
    DEBUG_ASSERT(triangles.size() <= max_triangle_count);

    return SimpleMesh(triangles, positions);
}

Expected<SimpleMesh> transform_mesh(SimpleMesh&& source_mesh, const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs)
{
    auto transform = srs::transformation(source_srs, target_srs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    if (auto result = srs::transform_points_inplace(transform->get(), source_mesh.positions); !result) {
        return Error::propagate(std::move(result));
    }
    return std::move(source_mesh);
}
/*
SimpleMesh transform_mesh(const SimpleMesh &source_mesh, const OGRSpatialReference &source_srs, const OGRSpatialReference& target_srs) {
    SimpleMesh target_mesh;
    target_mesh.positions = srs::transform_points(source_srs, target_srs, source_mesh.positions);
    target_mesh.triangles = source_mesh.triangles;
    target_mesh.uvs = source_mesh.uvs;
    target_mesh.texture = source_mesh.texture;
    return target_mesh;
}
*/

Expected<std::vector<glm::dvec2>> generate_uv_space(const std::vector<glm::dvec3>& positions,
    const OGRSpatialReference& mesh_srs,
    const OGRSpatialReference& texture_srs,
    radix::tile::SrsBounds& texture_bounds)
{
    auto transform = srs::transformation(mesh_srs, texture_srs);
    if (!transform) {
        return Error::propagate(std::move(transform));
    }
    auto transformed = srs::transform_points_to_2d(transform->get(), positions);
    if (!transformed) {
        return Error::propagate(std::move(transformed));
    }
    std::vector<glm::dvec2> uvs = std::move(*transformed);
    texture_bounds = radix::tile::SrsBounds(radix::geometry::find_bounds(std::span<const glm::dvec2>(uvs)));

    for (glm::dvec2 &uv : uvs) {
        uv = (uv - texture_bounds.min) / texture_bounds.size();
    }

    return uvs;
}

BuildMeshError log_transformation_error(const Error& error)
{
    LOG_ERROR("Mesh transformation failed: {}", error.to_string());
    return BuildMeshError::TransformationFailed;
}

// Shifts longitude bounds in degrees by whole turns into the turn starting at west_edge,
// splitting them at its end.
std::vector<radix::tile::SrsBounds> wrap_longitudes(const radix::tile::SrsBounds& bounds, const double west_edge)
{
    constexpr double turn = 360;
    const double west = bounds.min.x - turn * std::floor((bounds.min.x - west_edge) / turn);
    const double east = west + bounds.width();
    if (east <= west_edge + turn) {
        return { { { west, bounds.min.y }, { east, bounds.max.y } } };
    }
    return { { { west, bounds.min.y }, { west_edge + turn, bounds.max.y } }, { { west_edge, bounds.min.y }, { east - turn, bounds.max.y } } };
}

// Appends the meshes without merging vertices.
SimpleMesh concatenate(std::vector<SimpleMesh>&& meshes)
{
    if (meshes.size() == 1) {
        return std::move(meshes.front());
    }
    SimpleMesh result;
    for (const SimpleMesh& mesh : meshes) {
        const glm::uvec3 offset(static_cast<glm::uint>(result.positions.size()));
        for (const glm::uvec3& triangle : mesh.triangles) {
            result.triangles.push_back(triangle + offset);
        }
        result.positions.insert(result.positions.end(), mesh.positions.begin(), mesh.positions.end());
    }
    return result;
}
} // namespace

Expected<std::vector<radix::tile::SrsBounds>> native_read_windows(
    const OGRSpatialReference& dataset_srs, const radix::tile::SrsBounds& native_bounds, const radix::geometry::Aabb3d& ecef_bounds)
{
    auto coverage = srs::ecef2srs_coverage(ecef_bounds, dataset_srs);
    if (!coverage) {
        return Error::propagate(std::move(coverage));
    }
    // Geographic coverage is in degrees, within [-180, 180].
    const double west_edge = native_bounds.centre().x - 180;
    std::vector<radix::tile::SrsBounds> windows;
    for (const radix::geometry::Aabb3d& bounds : *coverage) {
        const radix::tile::SrsBounds bounds_2d(bounds);
        const auto parts = dataset_srs.IsGeographic() ? wrap_longitudes(bounds_2d, west_edge) : std::vector { bounds_2d };
        for (const auto& part : parts) {
            const radix::tile::SrsBounds window(glm::max(part.min, native_bounds.min), glm::min(part.max, native_bounds.max));
            if (window.min.x < window.max.x && window.min.y < window.max.y) {
                windows.push_back(window);
            }
        }
    }
    return windows;
}

std::expected<SimpleMesh, BuildMeshError> build_reference_mesh_patch(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &clip_srs, const radix::geometry::Aabb3d &clip_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds) {
    const auto source_srs_result = dataset.srs();
    if (!source_srs_result) {
        return std::unexpected(log_transformation_error(source_srs_result.error()));
    }
    const OGRSpatialReference& source_srs = *source_srs_result;

    // Coverage in the source srs is computed from ECEF bounds.
    const bool unbounded_height = !std::isfinite(clip_bounds.min.z) || !std::isfinite(clip_bounds.max.z);
    radix::geometry::Aabb3d ecef_clip_bounds = clip_bounds;
    if (const auto ecef_srs = srs::ecef(); !clip_srs.IsSame(&ecef_srs)) {
        // The source srs is 2D, so the windows for bounds unbounded in height don't depend on the
        // height they are computed at.
        radix::geometry::Aabb3d finite_clip_bounds = clip_bounds;
        if (!std::isfinite(finite_clip_bounds.min.z)) {
            finite_clip_bounds.min.z = std::isfinite(clip_bounds.max.z) ? clip_bounds.max.z : 0;
        }
        if (!std::isfinite(finite_clip_bounds.max.z)) {
            finite_clip_bounds.max.z = finite_clip_bounds.min.z;
        }
        const auto ecef_bounds = srs::ecef_coverage(clip_srs, finite_clip_bounds);
        if (!ecef_bounds) {
            return std::unexpected(log_transformation_error(ecef_bounds.error()));
        }
        ecef_clip_bounds = *ecef_bounds;
    }

    // Find what data to read in the source srs.
    const auto native_bounds = dataset.bounds();
    if (!native_bounds) {
        return std::unexpected(log_transformation_error(native_bounds.error()));
    }
    const auto windows = native_read_windows(source_srs, *native_bounds, ecef_clip_bounds);
    if (!windows) {
        return std::unexpected(log_transformation_error(windows.error()));
    }

    // Read height data in each window directly from dataset (no interpolation).
    RawDatasetReader reader(dataset);
    const float no_data_value = reader.get_no_data_value();
    bool any_read = false;
    std::vector<SimpleMesh> meshes_in_source_srs;
    for (const radix::tile::SrsBounds& window : *windows) {
        radix::geometry::Aabb2i pixel_bounds = reader.transform_srs_bounds_to_pixel_bounds(window);
        add_border_to_aabb(pixel_bounds, Border(1));
        LOG_TRACE("Reading pixels [({}, {})-({}, {})] from dataset", pixel_bounds.min.x, pixel_bounds.min.y, pixel_bounds.max.x, pixel_bounds.max.y);
        auto read_result = reader.read_data_in_pixel_bounds_clamped(pixel_bounds);
        if (!read_result.has_value()) {
            return std::unexpected(BuildMeshError::OutOfBounds);
        }
        if (read_result->buffer().empty()) {
            continue;
        }
        any_read = true;
        const radix::Raster<float> height_map = std::move(*read_result);

        LOG_TRACE("Finding valid pixels");
        const radix::RasterMask valid_mask = radix::raster::transform(height_map, [=](const float height) { return height != no_data_value; });

        LOG_TRACE("Transforming pixels to vertices");
        auto source_points_result = radix::raster::transform(height_map, valid_mask, [&](const float height, const glm::uvec2& coords) {
            return convert_pixel_to_vertex(height, coords, reader, pixel_bounds);
        });
        DEBUG_ASSERT(source_points_result.has_value());
        if (!source_points_result.has_value())
            return std::unexpected(BuildMeshError::EmptyRegion);
        const auto source_points = std::move(*source_points_result);

        LOG_TRACE("Generating triangles");
        SimpleMesh mesh = meshify(source_points, valid_mask);
        // Check if we even have any valid vertices. Can happen if all of the region is padding.
        if (mesh.vertex_count() != 0 && mesh.face_count() != 0) {
            meshes_in_source_srs.push_back(std::move(mesh));
        }
    }
    if (!any_read) {
        return std::unexpected(BuildMeshError::OutOfBounds);
    }
    if (meshes_in_source_srs.empty()) {
        return std::unexpected(BuildMeshError::EmptyRegion);
    }
    SimpleMesh mesh_in_source_srs = concatenate(std::move(meshes_in_source_srs));

    // Fast check if all vertices will be clipped. Horizontally, the windows already are close to
    // the clip bounds, so this only applies to their heights.
    if (!unbounded_height) {
        const radix::geometry::Aabb3d actual_source_bounds = calculate_bounds(mesh_in_source_srs);
        const auto actual_ecef_bounds = srs::ecef_coverage(source_srs, actual_source_bounds);
        if (!actual_ecef_bounds) {
            return std::unexpected(log_transformation_error(actual_ecef_bounds.error()));
        }
        if (!radix::geometry::intersect(*actual_ecef_bounds, ecef_clip_bounds)) {
            return std::unexpected(BuildMeshError::EmptyRegion);
        }
    }

    LOG_TRACE("Clipping mesh based on target bounds");
    const auto mesh_in_clip_srs = transform_mesh(std::move(mesh_in_source_srs), source_srs, clip_srs);
    if (!mesh_in_clip_srs) {
        return std::unexpected(log_transformation_error(mesh_in_clip_srs.error()));
    }
    SimpleMesh clipped_mesh = mesh::clip_on_bounds(*mesh_in_clip_srs, clip_bounds);
    // Check if there are any vertices left
    if (clipped_mesh.vertex_count() == 0 || clipped_mesh.face_count() == 0) {
        return std::unexpected(BuildMeshError::EmptyRegion);
    }

    // TODO: move this to another function?
    LOG_TRACE("Generating uv space and calculating required texture bounds");
    auto uvs = generate_uv_space(clipped_mesh.positions, clip_srs, texture_srs, texture_bounds);
    if (!uvs) {
        return std::unexpected(log_transformation_error(uvs.error()));
    }
    clipped_mesh.uvs = std::move(*uvs);

    LOG_TRACE("Transforming mesh into output srs");
    auto target_mesh_result = transform_mesh(std::move(clipped_mesh), clip_srs, mesh_srs);
    if (!target_mesh_result) {
        return std::unexpected(log_transformation_error(target_mesh_result.error()));
    }
    SimpleMesh target_mesh = std::move(*target_mesh_result);

    mesh::remove_isolated_vertices(target_mesh); // TODO: is this still required?
    mesh::validate(target_mesh);
    return target_mesh;
}

}
