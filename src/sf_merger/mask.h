#pragma once

#include <cmath>
#include <filesystem>
#include <map>
#include <vector>

#include <gdal_priv.h>

#include "Dataset.h"
#include "mesh/SimpleMesh.h"
#include "mesh/cgal.h"
#include "mesh/convert.h"
#include "mesh/validate.h"
#include "mesh/bounds.h"
#include "srs.h"
#include "SphereProjector.h"

#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Multipolygon_with_holes_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_repair/repair.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>

using Kernel = cgal::kernel::epeck::Kernel;
using Point2 = Kernel::Point_2;
using Polygon2 = CGAL::Polygon_2<Kernel>;
using PolygonWithHoles2 = CGAL::Polygon_with_holes_2<Kernel>;
using PolygonSet2 = CGAL::Polygon_set_2<Kernel>;
using MultipolygonWithHoles2 = CGAL::Multipolygon_with_holes_2<Kernel>;

struct ReferencedPolygonMask {
    MultipolygonWithHoles2 polygons;
    OGRSpatialReference srs;
};

struct SpherePolygonMask {
    MultipolygonWithHoles2 polygons;
    SphereProjector projector;
};

struct SphereMeshMask {
    SimpleMesh3d mesh;
};

struct MeshMask {
    SimpleMesh3d mesh;
};

namespace mask {

namespace {
    template <glm::length_t n_dims, typename T>
    static glm::vec<n_dims, T> scale_to_length(const glm::vec<n_dims, T> &v, const T target_length) {
        const T len = glm::length(v);
        if (len == T(0)) {
            return glm::vec<n_dims, T>(T(0));
        }
        return v * (target_length / len);
    }
}

enum class LoadErrorKind {
    UnsupportedFormat,
    FileNotFound,
    EmptySource
};

class LoadError {
public:
    LoadError() = default;
    constexpr LoadError(LoadErrorKind kind)
        : kind(kind) {}

    operator LoadErrorKind() const {
        return this->kind;
    }
    constexpr bool operator==(LoadError other) const {
        return this->kind == other.kind;
    }
    constexpr bool operator!=(LoadError other) const {
        return this->kind != other.kind;
    }

    std::string description() const {
        switch (kind) {
        case LoadErrorKind::UnsupportedFormat:
            return "format not supported";
        case LoadErrorKind::FileNotFound:
            return "file not found";
        case LoadErrorKind::EmptySource:
            return "empty input source";
        default:
            return "unknown error";
        }
    }

    friend std::ostream &operator<<(std::ostream &os, const LoadError &err) {
        return os << err.description();
    }

private:
    LoadErrorKind kind;
};

namespace {
std::optional<Polygon2> convert_ring(const OGRLinearRing &ring, bool is_outer) {
    uint32_t num_points = ring.getNumPoints();
    if (ring.get_IsClosed()) {
        num_points--;
    }
    if (num_points < 3) {
        return std::nullopt;
    }

    Polygon2 polygon;
    for (uint32_t i = 0; i < num_points; i++) {
        polygon.push_back(Point2(ring.getX(i), ring.getY(i)));
    }

    if (!polygon.is_simple()) {
        // Contains self intersections or duplicate points
        LOG_WARN("Skipping non-simple polygon");
        return std::nullopt;
    }

    if (is_outer != polygon.is_counterclockwise_oriented()) {
        polygon.reverse_orientation();
    }

    return polygon;
}

std::optional<PolygonWithHoles2> convert_polygon(const OGRPolygon &ogr_polygon) {
    const OGRLinearRing *outer_ring = ogr_polygon.getExteriorRing();
    DEBUG_ASSERT(outer_ring);

    auto outer_opt = convert_ring(*outer_ring, true);
    if (!outer_opt) {
        return std::nullopt;
    }
    const Polygon2 outer = std::move(outer_opt.value());

    PolygonWithHoles2 polygon(outer);
    const uint32_t num_holes = static_cast<uint32_t>(ogr_polygon.getNumInteriorRings());
    for (uint32_t i = 0; i < num_holes; i++) {
        const OGRLinearRing *inner = ogr_polygon.getInteriorRing(i);
        DEBUG_ASSERT(inner);
        auto hole_opt = convert_ring(*inner, false);
        if (!hole_opt) {
            continue;
        }
        Polygon2 hole = std::move(hole_opt.value());
        polygon.add_hole(std::move(hole));
    }

    return polygon;
}

void process_geometry(const OGRGeometry &geometry, MultipolygonWithHoles2 &out) {
    const OGRwkbGeometryType geometry_type = wkbFlatten(geometry.getGeometryType()); // map 2.5d to 2d

    switch (geometry_type) {
    case wkbPolygon: {
        const OGRPolygon *polygon = geometry.toPolygon();
        DEBUG_ASSERT(polygon);
        if (auto result = convert_polygon(*polygon)) {
            out.add_polygon_with_holes(std::move(*result));
        }
        break;
    }

    case wkbMultiPolygon:
    case wkbGeometryCollection: {
        const OGRGeometryCollection *collection = geometry.toGeometryCollection();
        DEBUG_ASSERT(collection);
        const uint32_t num_children = collection->getNumGeometries();
        for (uint32_t i = 0; i < num_children; i++) {
            process_geometry(*collection->getGeometryRef(i), out);
        }
        break;
    }

    default:
        LOG_WARN("Skipping unsupported geometry type: {}", OGRGeometryTypeToName(geometry_type));
        break;
    }
}

void convert_to_ecef_and_project_onto_sphere(MultipolygonWithHoles2 &polygons, const SphereProjector &projector, const OGRSpatialReference &srs) {
    const auto srs_transform = srs::transformation(srs, srs::ecef());
    for (auto &polygon : polygons.polygons_with_holes()) {
        for (auto &cgal_point : polygon.outer_boundary()) {
            const glm::dvec2 source_point = convert::to_glm_point(cgal_point);
            const glm::dvec3 ecef_point = srs::transform_point(srs_transform.get(), glm::dvec3(source_point, 0));
            const glm::dvec2 projected_point = projector.project_point(ecef_point);
            cgal_point = convert::to_cgal_point<Kernel>(projected_point);
        }
        for (auto &hole : polygon.holes()) {
            for (auto &cgal_point : hole) {
                const glm::dvec2 source_point = convert::to_glm_point(cgal_point);
                const glm::dvec3 ecef_point = srs::transform_point(srs_transform.get(), glm::dvec3(source_point, 0));
                const glm::dvec2 projected_point = projector.project_point(ecef_point);
                cgal_point = convert::to_cgal_point<Kernel>(projected_point);
            }
        }
    }
}

template <typename Vec>
auto length2(const Vec &v) -> decltype(glm::dot(v, v)) {
    return glm::dot(v, v);
}

glm::dvec2 calculate_radius_squared_range(const SimpleMesh3d &mesh) {
    const double infinity = std::numeric_limits<double>::infinity();
    double min_radius_sq = +infinity;
    double max_radius_sq = -infinity;

    for (const glm::dvec3 &position : mesh.positions) {
        const double radius_sq = length2(position);
        min_radius_sq = std::min(min_radius_sq, radius_sq);
        max_radius_sq = std::max(max_radius_sq, radius_sq);
    }

    return glm::dvec2(min_radius_sq, max_radius_sq);
}
glm::dvec2 calculate_radius_range(const SimpleMesh3d &mesh) {
    return glm::sqrt(calculate_radius_squared_range(mesh));
}

glm::dvec2 calculate_radius_squared_range(const std::span<const SimpleMesh3d> meshes) {
    const double infinity = std::numeric_limits<double>::infinity();
    double min_radius_sq = +infinity;
    double max_radius_sq = -infinity;

    for (const SimpleMesh3d &mesh : meshes) {
        const auto radius_sq_range = calculate_radius_squared_range(mesh);
        min_radius_sq = std::min(min_radius_sq, radius_sq_range.x);
        max_radius_sq = std::max(max_radius_sq, radius_sq_range.y);
    }

    return glm::dvec2(min_radius_sq, max_radius_sq);
}
glm::dvec2 calculate_radius_range(const std::span<const SimpleMesh3d> meshes) {
    return glm::sqrt(calculate_radius_squared_range(meshes));
}

} // namespace

inline tl::expected<ReferencedPolygonMask, LoadError> load_referenced_from_dataset(Dataset& mask_dataset) {
    GDALDataset *dataset = mask_dataset.gdalDataset();

    MultipolygonWithHoles2 polygons;
    for (auto &&feature_layer_pair : dataset->GetFeatures()) {
        OGRGeometry *geometry = feature_layer_pair.feature->GetGeometryRef();
        DEBUG_ASSERT(geometry);
        process_geometry(*geometry, polygons);
    }

    if (polygons.is_empty()) {
        LOG_ERROR("No valid polygons found in mask dataset '{}'", mask_dataset.name());
        return tl::unexpected(LoadErrorKind::EmptySource);
    }

    OGRSpatialReference srs;
    // TODO: remove this try catch
    try {
        srs = mask_dataset.srs();
    } catch (std::runtime_error &e) {
        LOG_WARN("Mask does not reference an srs, assuming WGS84");
        srs = srs::wgs84();
        // srs.SetAxisMappingStrategy(OAMS_AUTHORITY_COMPLIANT);
    }

    return ReferencedPolygonMask{.polygons = std::move(polygons), .srs = std::move(srs)};
}

inline SpherePolygonMask project_onto_sphere(ReferencedPolygonMask ref_mask, const double radius) {
    auto mask_2d = std::move(ref_mask.polygons);
    auto srs = std::move(ref_mask.srs);

    // Translate to ecef and project onto sphere
    const auto source_bounds_cgal = mask_2d.bbox();
    const radix::geometry::Aabb3d source_bounds(
        glm::dvec3(source_bounds_cgal.xmin(), source_bounds_cgal.ymin(), 0),
        glm::dvec3(source_bounds_cgal.xmax(), source_bounds_cgal.ymax(), 0));
    const radix::geometry::Aabb3d ecef_bounds = srs::non_exact_bounds_transform(source_bounds, srs, srs::ecef());
    const glm::dvec3 bounds_center = ecef_bounds.centre();
    const glm::dvec3 tangent_point = scale_to_length(bounds_center, radius);
    const SphereProjector projector(tangent_point);
    convert_to_ecef_and_project_onto_sphere(mask_2d, projector, srs);
    return SpherePolygonMask(mask_2d, projector);
}

// Triangulation type definitions
using VertexBase = CGAL::Triangulation_vertex_base_2<Kernel>;
using FaceBase = CGAL::Constrained_triangulation_face_base_2<Kernel>;
using TDS = CGAL::Triangulation_data_structure_2<VertexBase, FaceBase>;
using ExactTag = CGAL::Exact_predicates_tag;
using CDT = CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS, ExactTag>;
using FaceHandle = CDT::Face_handle;
using VertexHandle = CDT::Vertex_handle;

inline SphereMeshMask triangulate(const SpherePolygonMask &mask) {
    // Fix self intersections
    const MultipolygonWithHoles2 polygons = CGAL::Polygon_repair::repair(mask.polygons);

    CDT cdt;
    for (const auto &polygon : polygons.polygons_with_holes()) {
        const auto &boundary = polygon.outer_boundary();
        cdt.insert_constraint(boundary.vertices_begin(), boundary.vertices_end(), true);

        for (const auto &hole : polygon.holes()) {
            cdt.insert_constraint(hole.vertices_begin(), hole.vertices_end(), true);
        }
    }

    std::unordered_map<FaceHandle, bool> in_domain_map;
    boost::associative_property_map<std::unordered_map<FaceHandle, bool>> in_domain(in_domain_map);

    // Mark facets that are inside the domain bounded by the polygon
    CGAL::mark_domain_in_triangulation(cdt, in_domain);
    DEBUG_ASSERT(cdt.is_valid());

    // Build the resulting triangle mesh
    SimpleMesh3d result;
    // Map from CGAL vertex handles to output mesh indices
    std::unordered_map<VertexHandle, uint32_t> vertex_index_map;

    for (FaceHandle face : cdt.finite_face_handles()) {
        if (!get(in_domain, face)) {
            continue;
        }

        glm::uvec3 triangle;
        for (uint32_t i = 0; i < 3; i++) {
            const VertexHandle vertex = face->vertex(i);
            auto [it, inserted] = vertex_index_map.emplace(vertex, 0);
            if (inserted) {
                const uint32_t new_index = result.positions.size();
                const Point2 &cgal_point = vertex->point();
                const glm::dvec2 point2d = convert::to_glm_point(cgal_point);
                const glm::dvec3 point3d = mask.projector.unproject_point(point2d);
                result.positions.push_back(point3d);
                it->second = new_index;
            }
            triangle[i] = it->second;
        }

        result.triangles.push_back(triangle);
    }

    return SphereMeshMask(result);
}

namespace {
    struct ClosestFurthestCorners {
        glm::dvec3 closest;
        glm::dvec3 farthest;
    };

    ClosestFurthestCorners find_closest_and_farthest_corners(const radix::geometry::Aabb3d &bounds, const glm::dvec3 &point) {
        const glm::dvec3 dist_min = glm::abs(point - bounds.min);
        const glm::dvec3 dist_max = glm::abs(point - bounds.max);

        glm::dvec3 closest, farthest;

        for (uint32_t i = 0; i < 3; ++i) {
            if (dist_min[i] < dist_max[i]) {
                closest[i] = bounds.min[i];
                farthest[i] = bounds.max[i];
            } else {
                closest[i] = bounds.max[i];
                farthest[i] = bounds.min[i];
            }
        }

        return {closest, farthest};
    }
}

inline glm::dvec2 calculate_radius_range(const radix::geometry::Aabb3d& bounds) {
    const auto corners = find_closest_and_farthest_corners(bounds, glm::dvec3(0));
    return glm::dvec2(glm::length(corners.closest), glm::length(corners.farthest));
}

inline glm::dvec2 pad_radius_range(const glm::dvec2& radius_range, double padding_fraction) {
    DEBUG_ASSERT(radius_range.x >= 0 || radius_range.y >= 0);
    DEBUG_ASSERT(radius_range.y > radius_range.x);

    if (padding_fraction == 0.0) {
        return radius_range;
    }

    const double radius_range_center = (radius_range.x + radius_range.y) / 2;
    const double radius_range_extends = (radius_range.y - radius_range.x) / 2;
    const double padded_radius_range_extends = radius_range_extends * padding_fraction;
    return glm::dvec2(radius_range_center - padded_radius_range_extends, radius_range_center + padded_radius_range_extends);
}

inline MeshMask extrude(
    const SphereMeshMask &mask,
    const glm::dvec2& radius_range) {
    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mask.mesh);

    cgal::Mesh output_mesh;
    auto bottom = [&](cgal::VertexDescriptor input_vertex, cgal::VertexDescriptor output_vertex) {
        const cgal::Point3 input_point = cgal_mesh.point(input_vertex);
        const glm::dvec3 glm_input_point = convert::to_glm_point(input_point);
        const glm::dvec3 glm_offset_point = scale_to_length(glm_input_point, radius_range.x);
        const cgal::Point3 offset_point = convert::to_cgal_point<cgal::Kernel>(glm_offset_point);
        output_mesh.point(output_vertex) = offset_point;
    };
    auto top = [&](cgal::VertexDescriptor input_vertex, cgal::VertexDescriptor output_vertex) {
        const cgal::Point3 input_point = cgal_mesh.point(input_vertex);
        const glm::dvec3 glm_input_point = convert::to_glm_point(input_point);
        const glm::dvec3 glm_offset_point = scale_to_length(glm_input_point, radius_range.y);
        const cgal::Point3 offset_point = convert::to_cgal_point<cgal::Kernel>(glm_offset_point);
        output_mesh.point(output_vertex) = offset_point;
    };
    CGAL::Polygon_mesh_processing::extrude_mesh(cgal_mesh, output_mesh, bottom, top);

    return MeshMask(convert::to_simple_mesh(output_mesh));
}

inline MeshMask extrude(
    const SphereMeshMask &mask,
    const radix::geometry::Aabb3d &mesh_bounds,
    const double padding_fraction = 0.0) {
    const glm::dvec2 radius_range = calculate_radius_range(mesh_bounds);
    const glm::dvec2 padded_radius_range = pad_radius_range(radius_range, padding_fraction);
    return extrude(mask, padded_radius_range);
}

inline tl::expected<ReferencedPolygonMask, LoadError> load_referenced_from_path(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("Mask file does not exist: {}", path);
        return tl::unexpected(LoadErrorKind::FileNotFound);
    }

    auto ds_opt = Dataset::open_vector(path);
    if (!ds_opt.has_value()) {
        LOG_ERROR("Failed to load mask datset: {}", path);
        return tl::unexpected(LoadErrorKind::FileNotFound);
    }
    Dataset dataset = std::move(ds_opt.value());
    return load_referenced_from_dataset(dataset);
}

inline tl::expected<MeshMask, LoadError> load_from_path(const std::filesystem::path &path, const glm::dvec2& radius_range) {
    auto ref_mask_res = load_referenced_from_path(path);
    if (!ref_mask_res.has_value()) {
        return tl::unexpected(ref_mask_res.error());
    }
    ReferencedPolygonMask ref_polygon_mask = std::move(ref_mask_res.value());
    SpherePolygonMask sphere_polygon_mask = project_onto_sphere(std::move(ref_polygon_mask), radius_range.x);
    SphereMeshMask sphere_mesh_mask = triangulate(sphere_polygon_mask);
    MeshMask mesh_mask = extrude(sphere_mesh_mask, radius_range);

    return mesh_mask;
}

inline SpherePolygonMask compute_mesh_shadow(const SimpleMesh3d &mesh, const glm::dvec3 &tangent_point) {
    const SphereProjector projector(tangent_point);

    // Rotate each point into local frame and convert to lat lon
    std::vector<glm::dvec2> projected_vertices;
    projected_vertices.reserve(mesh.positions.size());
    for (const glm::dvec3 &vertex : mesh.positions) {
        projected_vertices.push_back(projector.project_point(vertex));
    }

    // Validate the mesh before processing
    SimpleMesh2d projected_mesh;
    projected_mesh.positions = projected_vertices;
    projected_mesh.triangles = mesh.triangles;
    mesh::validate(projected_mesh);

    // Then get the outer boundary (i.e. the mask) of this 2D mesh.
    // PolygonSet2 triangles;
    std::vector<Polygon2> triangles;
    triangles.reserve(mesh.triangles.size());
    Polygon2 triangle;
    triangle.reserve(3);
    for (const glm::uvec3 &indices : mesh.triangles) {
        triangle.clear();

        for (uint32_t i = 0; i < 3; i++) {
            const uint32_t vertex_index = indices[i];
            const glm::dvec2 position = projected_vertices[vertex_index];
            triangle.push_back(Point2(position.x, position.y));
        }

        if (!triangle.is_simple()) {
            // Contains self intersections or duplicate points
            continue;
        }

        // Ensure consistent orientation
        if (!triangle.is_counterclockwise_oriented()) {
            triangle.reverse_orientation();
        }

        triangles.push_back(triangle);
    }

    MultipolygonWithHoles2 polygons;
    CGAL::join(triangles.begin(), triangles.end(), std::back_inserter(polygons));
    return SpherePolygonMask(polygons, projector);
}

inline SpherePolygonMask compute_mesh_shadow(const SimpleMesh3d &mesh, const glm::dvec3 &direction, const double radius) {
    const glm::dvec3 tangent_point = scale_to_length(direction, radius);
    return compute_mesh_shadow(mesh, tangent_point);
}

inline SpherePolygonMask compute_mesh_shadow(const SimpleMesh3d &mesh, const double radius) {
    const radix::geometry::Aabb3d bounds = mesh::calculate_bounds(mesh);
    return compute_mesh_shadow(mesh, bounds.centre(), radius);
}

inline MeshMask create_from_mesh(const SimpleMesh3d &mesh, const glm::dvec3 &tangent_point, const glm::dvec2 &radius_range) {
    SpherePolygonMask sphere_polygon_mask = compute_mesh_shadow(mesh, tangent_point);
    SphereMeshMask sphere_mesh_mask = triangulate(sphere_polygon_mask);
    return extrude(sphere_mesh_mask, radius_range);
}

}
