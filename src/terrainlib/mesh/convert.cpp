#include "convert.h"
#include "log.h"

using namespace cgal;

glm::dvec3 convert::cgal2glm(Point3 point) {
    return glm::dvec3(CGAL::to_double(point[0]), CGAL::to_double(point[1]), CGAL::to_double(point[2]));
}
glm::dvec2 convert::cgal2glm(Point2 point) {
    return glm::dvec2(CGAL::to_double(point[0]), CGAL::to_double(point[1]));
}
Point3 convert::glm2cgal(glm::dvec3 point) {
    return Point3(point[0], point[1], point[2]);
}
Point2 convert::glm2cgal(glm::dvec2 point) {
    return Point2(point[0], point[1]);
}

SurfaceMesh convert::to_cgal_mesh(const SimpleMesh &mesh) {
    SurfaceMesh cgal_mesh;

    for (const glm::dvec3 &position : mesh.positions) {
        const CGAL::SM_Vertex_index vertex = cgal_mesh.add_vertex(glm2cgal(position));
        assert(vertex != SurfaceMesh::null_vertex());
    }

    for (const glm::uvec3 &triangle : mesh.triangles) {
        const CGAL::SM_Face_index face = cgal_mesh.add_face(
            CGAL::SM_Vertex_index(triangle.x),
            CGAL::SM_Vertex_index(triangle.y),
            CGAL::SM_Vertex_index(triangle.z));
        assert(face != SurfaceMesh::null_face());
    }

    return cgal_mesh;
}

SimpleMesh convert::to_simple_mesh(const SurfaceMesh &cgal_mesh) {
    SimpleMesh mesh;

    const size_t vertex_count = CGAL::num_vertices(cgal_mesh);
    const size_t face_count = CGAL::num_faces(cgal_mesh);
    mesh.positions.resize(vertex_count);
    mesh.triangles.reserve(face_count);

    for (const CGAL::SM_Vertex_index vertex_index : cgal_mesh.vertices()) {
        const Point3 &position = cgal_mesh.point(vertex_index);
        mesh.positions[vertex_index] = cgal2glm(position);
    }

    for (const CGAL::SM_Face_index face_index : cgal_mesh.faces()) {
        glm::uvec3 triangle;
        unsigned int i = 0;
        for (const CGAL::SM_Vertex_index vertex_index : CGAL::vertices_around_face(cgal_mesh.halfedge(face_index), cgal_mesh)) {
            triangle[i] = vertex_index;
            i++;
        }
        mesh.triangles.push_back(triangle);
    }

    return mesh;
}
