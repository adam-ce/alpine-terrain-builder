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
#include "mesh/terrain_mesh.h"
#include "srs.h"
#include "raster.h"
#include "log.h"
#include "mesh_builder.h"
#include "raw_dataset_reader.h"

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

TerrainMesh meshify(const raster::Raster<glm::dvec3>& source_points, const raster::Mask& mask) {
    // Compact the vertex grid into a list of valid ones.
    const size_t valid_vertex_count = std::reduce(mask.begin(), mask.end(), 0);
    // Check if we even have any valid vertices. Can happen if all of the region is padding.
    if (valid_vertex_count == 0) {
        return TerrainMesh();
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

    return TerrainMesh(triangles, positions);
}

TerrainMesh transform_mesh(const TerrainMesh &source_mesh, const OGRSpatialReference &source_srs, const OGRSpatialReference& target_srs) {
    TerrainMesh target_mesh;
    target_mesh.positions = srs::transform_points(source_srs, target_srs, source_mesh.positions);
    target_mesh.triangles = source_mesh.triangles;
    target_mesh.uvs = source_mesh.uvs;
    target_mesh.texture = source_mesh.texture;
    return target_mesh;
}

TerrainMesh reindex_mesh(const TerrainMesh &mesh) {
    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(mesh.vertex_count());
    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(mesh.face_count());
    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    std::vector<uint32_t> index_map(mesh.positions.size(), invalid_index);

    for (const auto &triangle : mesh.triangles) {
        glm::uvec3 new_triangle_indices;
        for (size_t i = 0; i < 3; i++) {
            const uint32_t old_index = triangle[i];
            if (index_map[old_index] == invalid_index) {
                // Vertex newly encountered
                const uint32_t new_index = new_positions.size();
                new_positions.push_back(mesh.positions[old_index]);
                new_triangle_indices[i] = new_index;
                index_map[old_index] = new_index;
            } else {
                // Vertex already encountered
                new_triangle_indices[i] = index_map[old_index];
            }
        }
        new_triangles.push_back(new_triangle_indices);
    }

    return TerrainMesh(new_triangles, new_positions);
}

namespace {
    struct DVec3Hash {
        std::size_t operator()(const glm::dvec3 &v) const {
            std::size_t h1 = std::hash<double>{}(v.x);
            std::size_t h2 = std::hash<double>{}(v.y);
            std::size_t h3 = std::hash<double>{}(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    struct DVec3Equal {
        const double epsilon;

        bool operator()(const glm::dvec3 &a, const glm::dvec3 &b) const {
            return glm::all(glm::epsilonEqual(a, b, 1e-8));
        }
    };
}

TerrainMesh clip_mesh(const TerrainMesh &mesh, const radix::geometry::Aabb3d &bounds) {
    if (mesh.vertex_count() == 0 || mesh.face_count() == 0) {
        return {};
    }

    // Calculate epsilon to merge newly created vertices
    const double max_edge_length = calculate_max_edge_length(mesh).value();
    const double average_edge_length = estimate_average_edge_length(mesh).value();
    const double epsilon = average_edge_length / 1000;

    std::unordered_map<glm::dvec3, size_t, DVec3Hash, DVec3Equal> seen_vertices(mesh.positions.size(), DVec3Hash(), DVec3Equal(epsilon));

    // Construct 6 axis-aligned clipping planes from the bounding box
    using Plane = radix::geometry::Plane<double>;
    const std::array<Plane, 6> planes = {
        Plane(glm::dvec3(1.0, 0.0, 0.0), -bounds.min.x), // left
        Plane(glm::dvec3(-1.0, 0.0, 0.0), bounds.max.x), // right
        Plane(glm::dvec3(0.0, 1.0, 0.0), -bounds.min.y), // bottom
        Plane(glm::dvec3(0.0, -1.0, 0.0), bounds.max.y), // top
        Plane(glm::dvec3(0.0, 0.0, 1.0), -bounds.min.z), // near
        Plane(glm::dvec3(0.0, 0.0, -1.0), bounds.max.z)  // far
    };

    std::vector<glm::dvec3> new_positions = mesh.positions;
    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(mesh.face_count());

    // Iterate over each triangle in the mesh
    for (const glm::uvec3 &source_triangle : mesh.triangles) {
        using Tri = radix::geometry::Triangle<3, double>;

        // Get the positions for the current triangle
        const Tri triangle = {
            mesh.positions[source_triangle.x],
            mesh.positions[source_triangle.y],
            mesh.positions[source_triangle.z]};

        const uint8_t inside_count = std::count_if(triangle.begin(), triangle.end(), [&](const auto &vertex) {
            return bounds.contains_inclusive(vertex);
        });
        if (inside_count == 0) {
            // Triangle vertices are not inside the bounds, however there can still be intersection
            if (std::any_of(triangle.begin(), triangle.end(), [&](const auto &vertex) {
                    return radix::geometry::distance_sq(bounds, vertex) > max_edge_length * max_edge_length;
                })) {
                continue;
            }
        }
        if (inside_count == source_triangle.length()) {
            new_triangles.push_back(source_triangle);
            continue;
        }

        // Start with the original triangle
        // TODO: this is rather inefficient since six vectors are allocated for each clipped triangle
        const std::vector<Tri> clipped_triangles = radix::geometry::clip(std::vector{triangle}, planes);
        for (const auto &clipped_triangle : clipped_triangles) {
            glm::uvec3 decomposed_triangle;
            for (size_t i = 0; i < clipped_triangle.size(); i++) {
                const auto &vertex = clipped_triangle[i];
                std::optional<uint32_t> vertex_index;

                // Check if this vertex was already in the source triangle
                for (size_t j = 0; j < triangle.size(); j++) {
                    const auto &source_vertex = triangle[j];
                    if (vertex == source_vertex) {
                        vertex_index = source_triangle[j];
                        break;
                    }
                }

                // Check if this vertex was already added
                if (!vertex_index.has_value()) {
                    const auto it = seen_vertices.find(vertex);
                    if (it != seen_vertices.cend()) {
                        vertex_index = it->second;
                    }

                }

                // Add a new vertex
                if (!vertex_index.has_value()) {
                    vertex_index = new_positions.size();
                    new_positions.push_back(vertex);
                    seen_vertices.emplace(vertex, vertex_index.value());
                }

                decomposed_triangle[i] = vertex_index.value();
            }
            new_triangles.push_back(decomposed_triangle);
        }
    }

    // TODO: derive new_positions from seen_vertices
    return reindex_mesh(TerrainMesh(new_triangles, new_positions));
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

tl::expected<TerrainMesh, BuildError> build_reference_mesh_tile(
    Dataset &dataset,
    const OGRSpatialReference &mesh_srs,
    const OGRSpatialReference &tile_srs, const radix::tile::SrsBounds &tile_bounds,
    const OGRSpatialReference &texture_srs, radix::tile::SrsBounds &texture_bounds) {
    return build_reference_mesh_patch(dataset, mesh_srs, tile_srs, extend_bounds_to_3d(tile_bounds), texture_srs, texture_bounds);
}

tl::expected<TerrainMesh, BuildError> build_reference_mesh_patch(
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
    const raster::Mask valid_mask = raster::transform(height_map, is_valid);

    LOG_TRACE("Transforming pixels to vertices");
    const raster::Raster<glm::dvec3> source_points = raster::transform(height_map, valid_mask, [&](const float height, const raster::Coords& coords) {
        return convert_pixel_to_vertex(height, coords, reader, pixel_bounds);
    });

    LOG_TRACE("Generating triangles");
    const TerrainMesh mesh_in_source_srs = meshify(source_points, valid_mask);
    // Check if we even have any valid vertices. Can happen if all of the region is padding.
    if (mesh_in_source_srs.vertex_count() == 0 || mesh_in_source_srs.face_count() == 0) {
        return tl::unexpected(BuildError::EmptyRegion);
    }

    LOG_TRACE("Clipping mesh based on target bounds");
    const TerrainMesh mesh_in_clip_srs = transform_mesh(mesh_in_source_srs, source_srs, clip_srs);
    TerrainMesh clipped_mesh = clip_mesh(mesh_in_clip_srs, clip_bounds);
    // Check if there are any vertices left
    if (clipped_mesh.vertex_count() == 0 || clipped_mesh.face_count() == 0) {
        return tl::unexpected(BuildError::EmptyRegion);
    }

    // TODO: move this to another function?
    LOG_TRACE("Generating uv space and calculating required texture bounds");
    clipped_mesh.uvs = generate_uv_space(clipped_mesh.positions, clip_srs, texture_srs, texture_bounds);

    LOG_TRACE("Transforming mesh into output srs");
    TerrainMesh target_mesh = transform_mesh(clipped_mesh, clip_srs, mesh_srs);
    
    remove_isolated_vertices(target_mesh); // TODO: is this still required?
    validate_mesh(target_mesh);
    return target_mesh;
}

} // namespace terrainbuilder::mesh
