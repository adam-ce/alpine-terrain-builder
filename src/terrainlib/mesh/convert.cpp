#include <cstddef>
#include <utility>
#include <optional>

#include <glm/glm.hpp>
#include <CGAL/boost/graph/helpers.h>
#include <CGAL/boost/graph/iterator.h>
#include <libassert/assert.hpp>

#include "mesh/convert.h"
#include "mesh/cgal.h"
#include "mesh/validate.h"
#include "log.h"

using UvMap = cgal::Mesh::Property_map<cgal::VertexIndex, glm::dvec2>;

cgal::Mesh convert::to_cgal_mesh(const SimpleMesh &mesh) {
    cgal::Mesh cgal_mesh;
    const size_t approx_num_edges = (mesh.face_count() * 3) / 2;
    cgal_mesh.reserve(mesh.vertex_count(), approx_num_edges, mesh.face_count());

    UvMap uv_map;
    if (mesh.has_uvs()) {
        auto [map, inserted] = cgal_mesh.add_property_map<cgal::VertexIndex, glm::dvec2>("v:uv");
        DEBUG_ASSERT(inserted);
        uv_map = std::move(map);
    }

    for (size_t index = 0; index < mesh.positions.size(); ++index) {
        const glm::dvec3 &position = mesh.positions[index];
        const cgal::VertexIndex vertex = cgal_mesh.add_vertex(to_cgal_point<cgal::Kernel>(position));
        DEBUG_ASSERT(vertex != cgal::Mesh::null_vertex());
        if (mesh.has_uvs()) {
            const glm::dvec2 &uv = mesh.uvs[index];
            uv_map[vertex] = uv;
        }
    }

    for (const glm::uvec3 &triangle : mesh.triangles) {
        const cgal::FaceIndex face = cgal_mesh.add_face(
            cgal::VertexIndex(triangle.x),
            cgal::VertexIndex(triangle.y),
            cgal::VertexIndex(triangle.z));
        USE(face);
        DEBUG_ASSERT(face != cgal::Mesh::null_face());
    }

    return cgal_mesh;
}

SimpleMesh convert::to_simple_mesh(const cgal::Mesh &cgal_mesh) {
    ASSERT(!cgal_mesh.has_garbage());
    
    SimpleMesh mesh;

    auto uv_map_opt = cgal_mesh.property_map<cgal::VertexIndex, glm::dvec2>("v:uv");
    const bool has_uvs = uv_map_opt.has_value();
    UvMap uv_map;
    if (has_uvs) {
        uv_map = std::move(uv_map_opt.value());
    }   

    const size_t vertex_count = CGAL::num_vertices(cgal_mesh);
    const size_t face_count = CGAL::num_faces(cgal_mesh);
    mesh.positions.resize(vertex_count);
    if (has_uvs) {
        mesh.uvs.resize(vertex_count);
    }
    mesh.triangles.reserve(face_count);

    for (const cgal::VertexIndex vertex_index : cgal_mesh.vertices()) {
        const cgal::Point3 &position = cgal_mesh.point(vertex_index);
        mesh.positions[vertex_index] = to_glm_point(position);
        if (has_uvs) {
            const glm::dvec2 &uv = uv_map[vertex_index];
            mesh.uvs[vertex_index] = uv;
        }
    }

    for (const cgal::FaceIndex face_index : cgal_mesh.faces()) {
        glm::uvec3 triangle;
        unsigned int i = 0;
        for (const cgal::VertexIndex vertex_index : CGAL::vertices_around_face(cgal_mesh.halfedge(face_index), cgal_mesh)) {
            triangle[i] = vertex_index;
            i++;
        }
        mesh.triangles.push_back(triangle);
    }

    mesh::validate(mesh);

    return mesh;
}
