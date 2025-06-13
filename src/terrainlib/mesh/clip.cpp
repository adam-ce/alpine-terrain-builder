#include <algorithm>
#include <array>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <radix/geometry.h>
#include <CGAL/Polygon_mesh_processing/clip.h>

#include "hash_utils.h"
#include "log.h"
#include "mesh/clip.h"
#include "mesh/utils.h"
#include "mesh/cgal.h"
#include "mesh/convert.h"

namespace {
double significant_above_epsilon(double x, double epsilon) {
    const double residual = std::fmod(x, epsilon);
    return x - residual;
}

bool is_degenerate(std::array<glm::dvec3, 3> triangle) {
    return triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[2] == triangle[0];
}

struct DVec3Hash {
    const double epsilon;

    std::size_t operator()(const glm::dvec3 &v) const {
        return hash_combine(
            significant_above_epsilon(v.x, epsilon),
            significant_above_epsilon(v.y, epsilon),
            significant_above_epsilon(v.z, epsilon));
    }
};

struct DVec3Equal {
    const double epsilon;

    bool operator()(const glm::dvec3 &a, const glm::dvec3 &b) const {
        return glm::all(glm::epsilonEqual(a, b, epsilon));
    }
};
} // namespace

SimpleMesh mesh::clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds) {
    assert(mesh.has_uvs() == false && "Clipping a mesh with UVs is not supported yet.");

    if (mesh.vertex_count() == 0 || mesh.face_count() == 0) {
        return {};
    }

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

    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(mesh.vertex_count());
    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(mesh.face_count());

    // Prepare a spatial hash map to deduplicate intersection vertices
    const double average_edge_length = estimate_average_edge_length(mesh, 100).value();
    const double epsilon = average_edge_length / 1000;
    std::unordered_map<glm::dvec3, uint32_t, DVec3Hash, DVec3Equal> seen_vertices(mesh.positions.size(), DVec3Hash(epsilon), DVec3Equal(epsilon));
    auto add_intersection_vertex = [&](const glm::dvec3& vertex) {
        const auto it = seen_vertices.find(vertex);
        if (it != seen_vertices.cend()) {
            return it->second;
        } else {
            const uint32_t vertex_index = new_positions.size();
            new_positions.push_back(vertex);
            seen_vertices.emplace(vertex, vertex_index);
            return vertex_index;
        }
    };

    // Create another simpler mapping for the vertices copied directly from the input mesh
    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    std::vector<uint32_t> position_mapping(mesh.vertex_count(), invalid_index);
    auto add_original_vertex = [&](const uint32_t original_index, const glm::dvec3 &vertex) {
        uint32_t &mapped = position_mapping[original_index];
        if (mapped == invalid_index) {
            mapped = new_positions.size();
            new_positions.emplace_back(vertex);
        }
        return mapped;
    };

    // Prepare temporary storage for intermediate triangles,
    // to avoid allocating a new vector for each triangle in the slow path.
    struct TriangleAndVertices {
        glm::uvec3 original_indices; // Indices of the vertices in the original mesh
        std::array<glm::dvec3, 3> vertices;
        uint8_t next_plane_to_clip; // Count which planes have already clipped this triangle
    };
    std::vector<TriangleAndVertices> triangles_left_to_clip;
    triangles_left_to_clip.reserve(6);

    // Iterate over each triangle in the mesh
    for (const glm::uvec3 &source_triangle : mesh.triangles) {
        // Get the positions for the current triangle
        std::array<glm::dvec3, 3> source_vertices = {
            mesh.positions[source_triangle.x],
            mesh.positions[source_triangle.y],
            mesh.positions[source_triangle.z]};

        // Start with a few quick checks to try to avoid the slow path
        const uint8_t in_bounds_count = std::count_if(source_vertices.begin(), source_vertices.end(), [&](const auto &vertex) {
            return bounds.contains_inclusive(vertex);
        });
        if (in_bounds_count == 0) {
            // Calculate the triangle bounds and perform an intersection check
            const glm::dvec3 triangle_min = glm::min(glm::min(source_vertices[0], source_vertices[1]), source_vertices[2]);
            const glm::dvec3 triangle_max = glm::max(glm::max(source_vertices[0], source_vertices[1]), source_vertices[2]);
            if (!radix::geometry::intersect(bounds, radix::geometry::Aabb3d{triangle_min, triangle_max})) {
                continue;
            }
        }
        if (in_bounds_count == 3) {
            // All vertices are in bounds, so we can directly add the vertex to the result mesh
            glm::uvec3 new_triangle;
            for (uint32_t i = 0; i < 3; i++) {
                new_triangle[i] = add_original_vertex(source_triangle[i], source_vertices[i]);
            }
            new_triangles.push_back(new_triangle);
            continue;
        }

        // Slow path: Clip triangle against all planes
        TriangleAndVertices current_triangle_and_vertices = {source_triangle, source_vertices, 0};
        triangles_left_to_clip.clear();

        // This outer loop iterates over the intermediate triangles potentially produced in the 2 inside 1 outside case
        // The current_triangle_and_vertices is updated isnide the switch or at the end of the loop
        while (true) {
            const glm::uvec3 &original_indices = current_triangle_and_vertices.original_indices;
            const std::array<glm::dvec3, 3> &vertices = current_triangle_and_vertices.vertices;
            bool skip_triangle = false;

            // Clip the current triangle against all planes
            for (uint8_t plane_index = current_triangle_and_vertices.next_plane_to_clip; plane_index < planes.size(); plane_index++) {
                const Plane &plane = planes[plane_index];

                // Check the distance of each vertex to the plane
                // and determine how many vertices are inside or outside the plane
                const std::array<double, 3> distance_to_plane = {
                    radix::geometry::distance(plane, vertices[0]),
                    radix::geometry::distance(plane, vertices[1]),
                    radix::geometry::distance(plane, vertices[2])};

                const std::array<bool, 3> vertex_inside = {
                    distance_to_plane[0] >= 0,
                    distance_to_plane[1] >= 0,
                    distance_to_plane[2] >= 0};
                const uint32_t inside_count = vertex_inside[0] + vertex_inside[1] + vertex_inside[2];

                // We need to use an if chain here since C++ does not like variable declarations in switch cases
                if (inside_count == 0) {
                    // All vertices are on the outside of the plane, so we skip the triangle
                    skip_triangle = true;
                    break;
                } else if (inside_count == 3) {
                    // All vertices are on the inside of the plane, so we keep the triangle as is
                    // and continue with the next plane
                    continue;
                } else if (inside_count == 1) {
                    // A single vertex is inside the plane, cut the other two off.

                    // First identify the vertices
                    uint32_t inside_tri_index, outside1_tri_index, outside2_tri_index;
                    for (uint32_t i = 0; i < 3; i++) {
                        if (vertex_inside[i]) {
                            inside_tri_index = i;
                            outside1_tri_index = (i + 1) % 3;
                            outside2_tri_index = (i + 2) % 3;
                            break;
                        }
                    }

                    // Skip the triangle if it only touches the plane
                    if (distance_to_plane[inside_tri_index] == 0) {
                        skip_triangle = true;
                        break;
                    }

                    // Then compute the intersections
                    const glm::dvec3 inside_vertex = vertices[inside_tri_index];
                    const glm::dvec3 outside1_vertex = vertices[outside1_tri_index];
                    const glm::dvec3 outside2_vertex = vertices[outside2_tri_index];

                    const glm::dvec3 intersection1 = radix::geometry::intersection(radix::geometry::Edge{inside_vertex, outside1_vertex}, plane);
                    const glm::dvec3 intersection2 = radix::geometry::intersection(radix::geometry::Edge{inside_vertex, outside2_vertex}, plane);

                    // Finally create the new triangle and use it as the current triangle
                    const glm::uvec3 new_triangle(
                        original_indices[inside_tri_index], invalid_index, invalid_index);
                    const std::array<glm::dvec3, 3> new_vertices = {
                        vertices[inside_tri_index],
                        intersection1,
                        intersection2};
                    if (is_degenerate(new_vertices)) {
                        skip_triangle = true;
                        break;
                    }
                    current_triangle_and_vertices = {new_triangle, new_vertices, static_cast<uint8_t>(plane_index + 1)};
                } else if (inside_count == 2) {
                    // Two vertices is inside the plane, cut the last one off and split the triangle.

                    // First identify the vertices
                    uint32_t outside_tri_index, inside1_tri_index, inside2_tri_index;
                    for (uint32_t i = 0; i < 3; i++) {
                        if (!vertex_inside[i]) {
                            outside_tri_index = i;
                            inside1_tri_index = (i + 1) % 3;
                            inside2_tri_index = (i + 2) % 3;
                            break;
                        }
                    }

                    // Then compute the intersections
                    const glm::dvec3 outside_vertex = vertices[outside_tri_index];
                    const glm::dvec3 inside1_vertex = vertices[inside1_tri_index];
                    const glm::dvec3 inside2_vertex = vertices[inside2_tri_index];

                    const glm::dvec3 intersection1 = radix::geometry::intersection(radix::geometry::Edge{outside_vertex, inside1_vertex}, plane);
                    const glm::dvec3 intersection2 = radix::geometry::intersection(radix::geometry::Edge{outside_vertex, inside2_vertex}, plane);

                    // Finally create the new triangles
                    // and use the first one as the current triangle
                    // while pushing the second one to the list of triangles left to clip
                    // quad order: outside - int1 - inside1 - inside2 - int2
                    const glm::uvec3 new_triangle1(
                        original_indices[inside1_tri_index], original_indices[inside2_tri_index], invalid_index);
                    const glm::uvec3 new_triangle2(
                        original_indices[inside2_tri_index], invalid_index, invalid_index);
                    const std::array<glm::dvec3, 3> new_vertices1 = {
                        inside1_vertex,
                        inside2_vertex,
                        intersection1};
                    const std::array<glm::dvec3, 3> new_vertices2 = {
                        inside2_vertex,
                        intersection2,
                        intersection1};
                    if (is_degenerate(new_vertices2)) {
                        current_triangle_and_vertices = {new_triangle1, new_vertices1, static_cast<uint8_t>(plane_index + 1)};
                    } else if (is_degenerate(new_vertices1)) {
                        current_triangle_and_vertices = {new_triangle2, new_vertices2, static_cast<uint8_t>(plane_index + 1)};
                    } else {
                        current_triangle_and_vertices = {new_triangle1, new_vertices1, static_cast<uint8_t>(plane_index + 1)};
                        triangles_left_to_clip.emplace_back(new_triangle2, new_vertices2, static_cast<uint8_t>(plane_index + 1));
                    }
                } else {
                    UNREACHABLE();
                }
            }

            if (skip_triangle) {
                break;
            }

            // If we reached here we have clipped triangle that was not discarded
            // However since it may contain new vertices not in the original mesh
            // we need to add them to the output mesh while avoiding duplicates
            glm::uvec3 indices;
            for (uint32_t i = 0; i < 3; i++) {
                const auto &original_vertex_index = original_indices[i];
                auto &vertex_index = indices[i];
                const auto &vertex = vertices[i];
                if (original_vertex_index == invalid_index) {
                    // We have a vertex not in the original mesh -> look up in the spatial hash map
                    vertex_index = add_intersection_vertex(vertex);
                } else {
                    // We have a vertex in the original mesh -> look up in the boolean vector
                    vertex_index = add_original_vertex(original_vertex_index, vertex);
                }
            }
            new_triangles.push_back(indices);

            // Continue with the next triangle if there are any left to clip
            if (triangles_left_to_clip.empty()) {
                break;
            } else {
                current_triangle_and_vertices = triangles_left_to_clip.back();
                triangles_left_to_clip.pop_back();
            }
        }
    }

    return SimpleMesh(new_triangles, new_positions);
}

SimpleMesh mesh::clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh &clip_mesh) {
    // Convert meshes to CGAL format
    cgal::SurfaceMesh cgal_mesh = convert::to_cgal_mesh(mesh);
    cgal::SurfaceMesh cgal_clip_mesh = convert::to_cgal_mesh(clip_mesh);

    // Perform clipping using CGAL
    const bool result = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh);
    if (!result) {
        throw std::runtime_error("CGAL::Polygon_mesh_processing::clip failed");
    }

    // Convert back to SimpleMesh format
    return convert::to_simple_mesh(cgal_mesh);
}
