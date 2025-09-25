#include "polygon/triangulate.h"

#include <libassert/assert.hpp>

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>

#include "mesh/cgal.h"
#include "mesh/convert.h"
#include "mesh/validate.h"
#include "mesh/utils.h"
#include "polygon/utils.h"

using Kernel = cgal::kernel::epeck::Kernel;
using Point2 = Kernel::Point_2;
using Polygon2 = CGAL::Polygon_2<Kernel>;

using VertexBase = CGAL::Triangulation_vertex_base_with_info_2<std::optional<uint32_t>, Kernel>;
using FaceBase = CGAL::Constrained_triangulation_face_base_2<Kernel>;
using TDS = CGAL::Triangulation_data_structure_2<VertexBase, FaceBase>;
using ExactTag = CGAL::Exact_predicates_tag;
using CDT = CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS, ExactTag>;
using FaceHandle = CDT::Face_handle;
using VertexHandle = CDT::Vertex_handle;

namespace polygon {
namespace {
// Compute orthonormal basis (u,v) of polygon plane
struct PlaneBasis {
    glm::dvec3 origin;
    glm::dvec3 u, v; // orthonormal basis vectors in plane
};

PlaneBasis make_basis(const Polygon3d &polygon) {
    const glm::dvec3& p0 = polygon.points[0];
    const glm::dvec3& p1 = polygon.points[1];
    const glm::dvec3& p2 = polygon.points[2];
    const glm::dvec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    const glm::dvec3 u = glm::normalize(p1 - p0);
    const glm::dvec3 v = glm::normalize(glm::cross(normal, u));
    return {p0, u, v};
}

// project 3d point onto 2d basis
glm::dvec2 project(const glm::dvec3 &p, const PlaneBasis &basis) {
    const glm::dvec3 d = p - basis.origin;
    return {glm::dot(d, basis.u), glm::dot(d, basis.v)};
}

// lift back to 3d
glm::dvec3 lift(const glm::dvec2 &q, const PlaneBasis &basis) {
    return basis.origin + q.x * basis.u + q.y * basis.v;
}

}

// TODO: deduplicate overloads
void triangulate(SimpleMesh3d &mesh, const std::span<const uint32_t> indices) {
    if (indices.size() < 3) {
        return;
    }

    // Build the 3D points of the polygon
    Polygon3d polygon;
    polygon.points.reserve(indices.size());
    for (const uint32_t vertex_index : indices) {
        DEBUG_ASSERT(vertex_index < mesh.positions.size());
        polygon.points.push_back(mesh.positions[vertex_index]);
    }

    // Holes are oriented CW
    std::reverse(polygon.points.begin(), polygon.points.end());

    DEBUG_ASSERT(polygon::is_planar(polygon));
    const PlaneBasis basis = make_basis(polygon);

    CDT cdt;

    // Insert all polygon vertices and assign their mesh indices
    std::vector<VertexHandle> handles;
    handles.reserve(indices.size());
    for (uint32_t i = 0; i < indices.size(); i++) {
        const glm::dvec3 &point = polygon.points[i];
        const glm::dvec2 projected = project(point, basis);
        VertexHandle vh = cdt.insert(convert::to_cgal_point<Kernel>(projected));
        vh->info() = indices[i];
        handles.push_back(vh);
    }

    // Insert constraints between consecutive vertices
    for (size_t i = 0; i < handles.size(); i++) {
        cdt.insert_constraint(handles[i], handles[(i + 1) % handles.size()]);
    }

    // Mark facets that are inside the domain bounded by the polygon (since the full convex hull is triangulated)
    std::unordered_map<FaceHandle, bool> in_domain_map;
    boost::associative_property_map<std::unordered_map<FaceHandle, bool>> in_domain(in_domain_map);
    CGAL::mark_domain_in_triangulation(cdt, in_domain);
    DEBUG_ASSERT(cdt.is_valid());

    // insert new triangles and vertices
    for (const FaceHandle face : cdt.finite_face_handles()) {
        if (!get(in_domain, face)) {
            continue; // outside polygon
        }

        glm::uvec3 triangle;
        for (int i = 0; i < 3; i++) {
            const auto &vertex = face->vertex(i);
            const auto &original_index = vertex->info();
            if (!original_index.has_value()) {
                const uint32_t new_index = mesh.positions.size();
                const Point2 &cgal_point = vertex->point();
                const glm::dvec2 point2d = convert::to_glm_point(cgal_point);
                const glm::dvec3 point3d = lift(point2d, basis);
                mesh.positions.push_back(point3d);
                if (mesh.has_uvs()) {
                    const glm::dvec2 uv = {0, 0};
                    mesh.uvs.push_back(uv);
                }
                triangle[i] = new_index;
            } else {
                triangle[i] = original_index.value();
            }
        }

        mesh.triangles.push_back(triangle);
    }
}

SimpleMesh3d triangulate(const Polygon3d &polygon) {
    DEBUG_ASSERT(polygon::is_planar(polygon));
    const PlaneBasis basis = make_basis(polygon);

    CDT cdt;
    // insert edges
    for (size_t i = 0; i < polygon.size(); i++) {
        const glm::dvec2 a = project(polygon.points[i], basis);
        const glm::dvec2 b = project(polygon.points[(i + 1) % polygon.size()], basis);
        cdt.insert_constraint(
            convert::to_cgal_point<Kernel>(a),
            convert::to_cgal_point<Kernel>(b)
        );
    }

    // Mark facets that are inside the domain bounded by the polygon (since the full convex hull is triangulated)
    std::unordered_map<FaceHandle, bool> in_domain_map;
    boost::associative_property_map<std::unordered_map<FaceHandle, bool>> in_domain(in_domain_map);
    CGAL::mark_domain_in_triangulation(cdt, in_domain);
    DEBUG_ASSERT(cdt.is_valid());

    // Build the resulting triangle mesh
    SimpleMesh3d result;
    // insert new triangles and vertices
    for (const FaceHandle face : cdt.finite_face_handles()) {
        if (!get(in_domain, face)) {
            continue; // outside polygon
        }

        glm::uvec3 triangle;
        for (int i = 0; i < 3; i++) {
            const auto &vertex = face->vertex(i);
            const auto &original_index = vertex->info();
            if (!original_index.has_value()) {
                const uint32_t new_index = result.positions.size();
                const Point2 &cgal_point = vertex->point();
                const glm::dvec2 point2d = convert::to_glm_point(cgal_point);
                const glm::dvec3 point3d = lift(point2d, basis);
                result.positions.push_back(point3d);
                triangle[i] = new_index;
            } else {
                triangle[i] = original_index.value();
            }
        }

        result.triangles.push_back(triangle);
    }

    return result;
}
}
