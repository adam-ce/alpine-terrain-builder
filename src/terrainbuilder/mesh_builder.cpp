#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

#include <gdal.h>
#include <gdal_priv.h>
#include <glm/glm.hpp>
#include <radix/geometry.h>
#include <glm/gtx/norm.hpp>

#include "Dataset.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "mesh_builder.h"
#include "raster.h"
#include "raw_dataset_reader.h"
#include "srs.h"
#include "mesh/utils.h"

namespace terrainbuilder::mesh {

std::ostream &operator<<(std::ostream &os, BuildError error) {
    switch (error) {
    case BuildError::OutOfBounds:
        os << "out of bounds";
        break;
    case BuildError::EmptyRegion:
        os << "empty region";
        break;
    default:
        os << "unknown build error";
        break;
    }
    return os;
}

template <typename T>
static glm::dvec2 apply_transform(std::array<double, 6> transform, const glm::tvec2<T> &v) {
    glm::dvec2 result;
    GDALApplyGeoTransform(transform.data(), v.x, v.y, &result.x, &result.y);
    return result;
}

using PixelBounds = radix::geometry::Aabb2i;

// TODO:: write documentation
// TODO: use referencedBounds

bool is_valid(float height) {
    return !isnan(height) && !isinf(height)      /* <- invalid values */
           && height > -20000 && height < 20000; /* <- padding */
}

glm::dvec3 convert_pixel_to_vertex(const float height, const raster::Coords pixel_coords, const RawDatasetReader& reader, const PixelBounds& pixel_bounds) {
    const glm::dvec2 point_offset_in_raster(0.5); // Convert pixel coordinates into a point in the dataset's srs.
    const glm::dvec2 coords_raster_relative = glm::dvec2(pixel_coords) + point_offset_in_raster;
    const glm::dvec2 coords_raster_absolute = coords_raster_relative + glm::dvec2(pixel_bounds.min);
    const glm::dvec3 coords_source(reader.transform_pixel_to_srs_point(coords_raster_absolute), height);
    return coords_source;
}

SimpleMesh meshify(const raster::Raster<glm::dvec3>& source_points, const raster::Mask& mask) {
    // Compact the vertex grid into a list of valid ones.
    const size_t valid_vertex_count = std::reduce(mask.begin(), mask.end(), 0);
    // Check if we even have any valid vertices. Can happen if all of the region is padding.
    if (valid_vertex_count == 0) {
        return SimpleMesh();
    }

    std::vector<glm::dvec3> positions;
    positions.reserve(valid_vertex_count);

    const raster::Raster<size_t> vertex_index_map = raster::transform(source_points, mask, [&](const glm::dvec3 &point) -> size_t {
        const size_t index = positions.size();
        positions.push_back(point);
        return index;
    });
    assert(positions.size() == valid_vertex_count);

    // Allocate triangle vector
    const size_t max_triangle_count = (source_points.width() - 1) * (source_points.height() - 1) * 2;
    std::vector<glm::uvec3> triangles;
    triangles.reserve(max_triangle_count);

    for (size_t x = 0; x < source_points.width() - 1; x++) {
        for (size_t y = 0; y < source_points.height() - 1; y++) {
            const std::array<raster::Coords, 4> quad {
                raster::Coords{x, y},
                raster::Coords{x + 1, y},
                raster::Coords{x + 1, y + 1},
                raster::Coords{x, y + 1}};

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
    assert(triangles.size() <= max_triangle_count);

    return SimpleMesh(triangles, positions);
}

SimpleMesh transform_mesh(const SimpleMesh &source_mesh, const OGRSpatialReference &source_srs, const OGRSpatialReference& target_srs) {
    SimpleMesh target_mesh;
    target_mesh.positions = srs::transform_points(source_srs, target_srs, source_mesh.positions);
    target_mesh.triangles = source_mesh.triangles;
    target_mesh.uvs = source_mesh.uvs;
    target_mesh.texture = source_mesh.texture;
    return target_mesh;
}

std::vector<glm::dvec2> generate_uv_space(const std::vector<glm::dvec3>& positions, const OGRSpatialReference &mesh_srs, const OGRSpatialReference &texture_srs, radix::tile::SrsBounds& texture_bounds) {
    std::vector<glm::dvec2> uvs = srs::transform_points_to_2d(srs::transformation(mesh_srs, texture_srs).get(), positions);
    texture_bounds = radix::tile::SrsBounds(radix::geometry::find_bounds(uvs));

    for (glm::dvec2 &uv : uvs) {
        uv = (uv - texture_bounds.min) / texture_bounds.size();
    }

    return uvs;
}

radix::geometry::Aabb3d extend_bounds_to_3d(radix::geometry::Aabb2d bounds2d) {
    const double infinity = std::numeric_limits<double>::infinity();
    const glm::dvec3 min(bounds2d.min, -infinity);
    const glm::dvec3 max(bounds2d.max, infinity);
    return radix::geometry::Aabb3d(min, max);
}

tl::expected<SimpleMesh, BuildError> build_reference_mesh_tile(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &tile_srs, const radix::tile::SrsBounds &tile_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds) {
    return build_reference_mesh_patch(dataset, mesh_srs, tile_srs, extend_bounds_to_3d(tile_bounds), texture_srs, texture_bounds);
}

tl::expected<SimpleMesh, BuildError> build_reference_mesh_patch(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &clip_srs, const radix::geometry::Aabb3d &clip_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds) {
    const OGRSpatialReference &source_srs = dataset.srs();

    // Translate tile bounds from tile srs into the source srs, so we know what data to read.
    radix::tile::SrsBounds target_bounds_in_source_srs;
    if (std::isinf(clip_bounds.min.z) && std::isinf(clip_bounds.max.z)) {
        // Make target bounds 2d if the unbounded by height.
        target_bounds_in_source_srs = srs::encompassing_bounds_transfer(clip_srs, source_srs, radix::tile::SrsBounds(clip_bounds));
    } else {
        target_bounds_in_source_srs = srs::encompassing_bounds_transfer(clip_srs, source_srs, clip_bounds);
    }

    // Read height data according to bounds directly from dataset (no interpolation).
    RawDatasetReader reader(dataset);
    radix::geometry::Aabb2i pixel_bounds = reader.transform_srs_bounds_to_pixel_bounds(target_bounds_in_source_srs);
    add_border_to_aabb(pixel_bounds, Border(1));
    LOG_TRACE("Reading pixels [({}, {})-({}, {})] from dataset", pixel_bounds.min.x, pixel_bounds.min.y, pixel_bounds.max.x, pixel_bounds.max.y);
    const std::optional<raster::HeightMap> read_result = reader.read_data_in_pixel_bounds(pixel_bounds, true);
    if (!read_result.has_value() || read_result->size() == 0) {
        return tl::unexpected(BuildError::OutOfBounds);
    }
    const raster::HeightMap height_map = read_result.value();

    LOG_TRACE("Finding valid pixels");
    const float no_data_value = reader.get_no_data_value();
    const raster::Mask valid_mask = raster::transform(height_map, [=](const float height) {
        return height != no_data_value;
    });
    // const raster::Mask valid_mask = raster::transform(height_map, is_valid);

    LOG_TRACE("Transforming pixels to vertices");
    const raster::Raster<glm::dvec3> source_points = raster::transform(height_map, valid_mask, [&](const float height, const raster::Coords& coords) {
        return convert_pixel_to_vertex(height, coords, reader, pixel_bounds);
    });

    LOG_TRACE("Generating triangles");
    const SimpleMesh mesh_in_source_srs = meshify(source_points, valid_mask);
    // Check if we even have any valid vertices. Can happen if all of the region is padding.
    if (mesh_in_source_srs.vertex_count() == 0 || mesh_in_source_srs.face_count() == 0) {
        return tl::unexpected(BuildError::EmptyRegion);
    }

    LOG_TRACE("Clipping mesh based on target bounds");
    const SimpleMesh mesh_in_clip_srs = transform_mesh(mesh_in_source_srs, source_srs, clip_srs);
    SimpleMesh clipped_mesh = clip_mesh_on_bounds(mesh_in_clip_srs, clip_bounds);
    // Check if there are any vertices left
    if (clipped_mesh.vertex_count() == 0 || clipped_mesh.face_count() == 0) {
        return tl::unexpected(BuildError::EmptyRegion);
    }

    // TODO: move this to another function?
    LOG_TRACE("Generating uv space and calculating required texture bounds");
    clipped_mesh.uvs = generate_uv_space(clipped_mesh.positions, clip_srs, texture_srs, texture_bounds);

    LOG_TRACE("Transforming mesh into output srs");
    SimpleMesh target_mesh = transform_mesh(clipped_mesh, clip_srs, mesh_srs);
    
    remove_isolated_vertices(target_mesh); // TODO: is this still required?
    validate_mesh(target_mesh);
    return target_mesh;
}

} // namespace terrainbuilder::mesh
