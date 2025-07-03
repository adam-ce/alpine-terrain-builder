#include <algorithm>
#include <array>
#include <vector>
#include <unordered_map>
#include <optional>

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "hash_utils.h"
#include "log.h"
#include "mesh/clip.h"
#include "mesh/utils.h"
#include "mesh/cgal.h"
#include "mesh/convert.h"
#include "mesh/validate.h"

namespace {
double significant_above_epsilon(double x, double epsilon) {
    const double residual = std::fmod(x, epsilon);
    return x - residual;
}

template <typename T>
bool is_degenerate(const T& triangle) {
    return triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[2] == triangle[0];
}

struct DVec3Hash {
    const double epsilon;

    std::size_t operator()(const glm::dvec3 &v) const {
        return hash::combine(
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

template <typename T>
struct Intersection {
    glm::tvec3<T> point;
    T t; // The parameter value along the line at which the intersection occurs
};

// Copy of radix::geometry::intersection function that outputs the t value.
template <typename T>
std::optional<Intersection<T>> compute_intersection(const radix::geometry::Line<3, T> &line, const radix::geometry::Plane<T> &plane) {
    const auto dot = glm::dot(plane.normal, line.direction);
    if (std::abs(dot) < radix::geometry::epsilon<T>) {
        return {};
    }
    const T t = (-plane.distance - glm::dot(plane.normal, line.point)) / dot;
    return Intersection(line.point + t * line.direction, t);
}

// Copy of radix::geometry::intersection function that outputs the t value.
template <typename T>
Intersection<T> compute_intersection(const radix::geometry::Edge<3, T> &line, const radix::geometry::Plane<T> &plane) {
    const auto direction = line[1] - line[0];
    return compute_intersection(radix::geometry::Line{line[0], direction}, plane).value();
}

} // namespace

SimpleMesh mesh::clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds) {
    if (mesh.vertex_count() == 0 || mesh.face_count() == 0) {
        return {};
    }

    if (mesh.has_uvs() && mesh.uvs.size() != mesh.vertex_count()) {
        LOG_ERROR_AND_EXIT("Mesh has Uvs, but the number of Uvs does not match the number of vertices.");
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
    std::vector<glm::dvec2> new_uvs;
    new_uvs.reserve(mesh.uvs.size());
    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(mesh.face_count());

    // Prepare a spatial hash map to deduplicate intersection vertices
    const double average_edge_length = estimate_average_edge_length(mesh, 100).value();
    const double epsilon = average_edge_length / 1000;
    std::unordered_map<glm::dvec3, uint32_t, DVec3Hash, DVec3Equal> seen_vertices(mesh.positions.size(), DVec3Hash(epsilon), DVec3Equal(epsilon));
    auto add_intersection_vertex = [&](const glm::dvec3& vertex, const glm::dvec2 &Uv) {
        const auto it = seen_vertices.find(vertex);
        if (it != seen_vertices.cend()) {
            return it->second;
        } else {
            const uint32_t vertex_index = new_positions.size();
            new_positions.push_back(vertex);
            if (mesh.has_uvs()) {
                new_uvs.push_back(Uv);
            }
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
            if (mesh.has_uvs()) {
                new_uvs.emplace_back(mesh.uvs[original_index]);
            }
        }
        return mapped;
    };

    // Prepare temporary storage for intermediate triangles,
    // to avoid allocating a new vector for each triangle in the slow path.
    struct TriangleAndVertices {
        glm::uvec3 original_indices; // Indices of the vertices in the original mesh
        std::array<glm::dvec3, 3> vertices;
        std::array<glm::dvec2, 3> uvs;
        uint8_t next_plane_to_clip; // Count which planes have already clipped this triangle
    };
    std::vector<TriangleAndVertices> triangles_left_to_clip;
    triangles_left_to_clip.reserve(6);

    // Iterate over each triangle in the mesh
    for (const glm::uvec3 &source_triangle : mesh.triangles) {
        // Get the positions for the current triangle
        const std::array<glm::dvec3, 3> source_vertices = {
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

        const std::array<glm::dvec2, 3> source_uvs = mesh.has_uvs() ? std::array<glm::dvec2, 3>{
            mesh.uvs[source_triangle.x],
            mesh.uvs[source_triangle.y],
            mesh.uvs[source_triangle.z]} : std::array<glm::dvec2, 3>{
            glm::dvec2(0.0, 0.0),
            glm::dvec2(0.0, 0.0),
            glm::dvec2(0.0, 0.0)};

        // Slow path: Clip triangle against all planes
        TriangleAndVertices current_triangle_and_vertices = {source_triangle, source_vertices, source_uvs, 0};
        triangles_left_to_clip.clear();

        // This outer loop iterates over the intermediate triangles potentially produced in the 2 inside 1 outside case
        // The current_triangle_and_vertices is updated isnide the switch or at the end of the loop
        while (true) {
            const glm::uvec3 &original_indices = current_triangle_and_vertices.original_indices;
            const std::array<glm::dvec3, 3> &vertices = current_triangle_and_vertices.vertices;
            const std::array<glm::dvec2, 3> &uvs = current_triangle_and_vertices.uvs;
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

                    const Intersection intersection1 = compute_intersection(radix::geometry::Edge{inside_vertex, outside1_vertex}, plane);
                    const Intersection intersection2 = compute_intersection(radix::geometry::Edge{inside_vertex, outside2_vertex}, plane);

                    // Compute the Uvs for the vertices and intersection points
                    const glm::dvec2 inside_uv = uvs[inside_tri_index];
                    const glm::dvec2 outside1_uv = uvs[outside1_tri_index];
                    const glm::dvec2 outside2_uv = uvs[outside2_tri_index];
                    const glm::dvec2 intersection1_uv = glm::mix(inside_uv, outside1_uv, intersection1.t);
                    const glm::dvec2 intersection2_uv = glm::mix(inside_uv, outside2_uv, intersection2.t);

                    // Finally create the new triangle and use it as the current triangle
                    const glm::uvec3 new_triangle(
                        original_indices[inside_tri_index], invalid_index, invalid_index);
                    const std::array<glm::dvec3, 3> new_vertices = {
                        inside_vertex,
                        intersection1.point,
                        intersection2.point};
                    const std::array<glm::dvec2, 3> new_uvs = {
                        inside_uv,
                        intersection1_uv,
                        intersection2_uv};
                    if (is_degenerate(new_vertices)) {
                        skip_triangle = true;
                        break;
                    }
                    current_triangle_and_vertices = {new_triangle, new_vertices, new_uvs, static_cast<uint8_t>(plane_index + 1)};
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

                    const Intersection intersection1 = compute_intersection(radix::geometry::Edge{outside_vertex, inside1_vertex}, plane);
                    const Intersection intersection2 = compute_intersection(radix::geometry::Edge{outside_vertex, inside2_vertex}, plane);

                    // Compute the Uvs for the vertices and intersection points
                    const glm::dvec2 outside_uv = uvs[outside_tri_index];
                    const glm::dvec2 inside1_uv = uvs[inside1_tri_index];
                    const glm::dvec2 inside2_uv = uvs[inside2_tri_index];
                    const glm::dvec2 intersection1_uv = glm::mix(outside_uv, inside1_uv, intersection1.t);
                    const glm::dvec2 intersection2_uv = glm::mix(outside_uv, inside2_uv, intersection2.t);

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
                        intersection1.point};
                    const std::array<glm::dvec3, 3> new_vertices2 = {
                        inside2_vertex,
                        intersection2.point,
                        intersection1.point};
                    const std::array<glm::dvec2, 3> new_uvs1 = {
                        inside1_uv,
                        inside2_uv,
                        intersection1_uv};    ;
                    const std::array<glm::dvec2, 3> new_uvs2 = {
                        inside2_uv,
                        intersection2_uv,
                        intersection1_uv};

                    if (is_degenerate(new_vertices2)) {
                        if (is_degenerate(new_vertices1)) {
                            skip_triangle = true;
                            break;
                        } else {
                            current_triangle_and_vertices = {new_triangle1, new_vertices1, new_uvs1, static_cast<uint8_t>(plane_index + 1)};
                        }
                    } else {
                        if (is_degenerate(new_vertices1)) {
                            current_triangle_and_vertices = {new_triangle2, new_vertices2, new_uvs2, static_cast<uint8_t>(plane_index + 1)};
                        } else {
                            current_triangle_and_vertices = {new_triangle1, new_vertices1, new_uvs1, static_cast<uint8_t>(plane_index + 1)};
                            triangles_left_to_clip.emplace_back(new_triangle2, new_vertices2, new_uvs2, static_cast<uint8_t>(plane_index + 1));
                        }
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
                const auto &uv = uvs[i];
                if (original_vertex_index == invalid_index) {
                    // We have a vertex not in the original mesh -> look up in the spatial hash map
                    vertex_index = add_intersection_vertex(vertex, uv);
                } else {
                    // We have a vertex in the original mesh -> look up in the boolean vector
                    vertex_index = add_original_vertex(original_vertex_index, vertex);
                }
            }
            DEBUG_ASSERT(!is_degenerate(indices));
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

    const SimpleMesh clipped_mesh(new_triangles, new_positions, new_uvs, mesh.texture);
    mesh::validate(clipped_mesh);
    return clipped_mesh;
}

namespace {
template <typename TriangleMesh, typename UvMap>
struct UvInterpolatorVisitor : public CGAL::Polygon_mesh_processing::Corefinement::Default_visitor<TriangleMesh> {
    using HalfedgeDescriptor = typename boost::graph_traits<TriangleMesh>::halfedge_descriptor;
    using VertexDescriptor = typename boost::graph_traits<TriangleMesh>::vertex_descriptor;
    using VertexIndex = typename TriangleMesh::Vertex_index;

    UvMap& uv_map;
    
    UvInterpolatorVisitor(UvMap& uv_map) : uv_map(uv_map) {}

    std::vector<std::pair<bool, HalfedgeDescriptor>> intersection_uvs;

    /*
    VertexDescriptor edge_split_source;
    VertexDescriptor edge_split_target;

    // called before the edge of h in tm is split.
    // Each subsequent call to edge_split() until the call to after_edge_split() will correspond to the split of that edge.
    // If edge_split(h_i, tm) is called for i=1 to n, h_1, h_2, ... ,h_n, h is the sequence of halfedges representing the edge split (with the same initial orientation).
    // There is only one call per edge.
    void before_edge_split(HalfedgeDescriptor h, const TriangleMesh &tm) {
        edge_split_source = tm.source(h);
        edge_split_target = tm.target(h);
    }

    // called when a new split is done. The target of hnew is a new split vertex. There is only one call per edge.
    void edge_split(HalfedgeDescriptor hnew, const TriangleMesh &tm) {
        // Calculate t value for the split vertex
        const auto old_source_position = tm.point(edge_split_source);
        const auto old_target_position = tm.point(edge_split_target);
        const auto new_position = tm.point(hnew);
        const auto t = glm::distance(new_position, old_source_position) / glm::distance(old_target_position, old_source_position);

        // Interpolate UVs for the split vertex based on the original vertices
        const auto old_source_uv = uv_map[edge_split_source];
        const auto old_target_uv = uv_map[edge_split_target];
        const auto new_uv = glm::mix(old_source_uv, old_target_uv, t);
        uv_map[tm.target(hnew)] = new_uv;
    }*/

    // called when a new intersection point is detected.
    // The intersection is detected using a face of tm_f and an edge of tm_e. 
    void intersection_point_detected(
        size_t i_id, int sdim,
        HalfedgeDescriptor h_e, HalfedgeDescriptor h_f, 
        const TriangleMesh& tm_e, const TriangleMesh& tm_f, bool is_target_coplanar, bool is_source_coplanar) {
        DEBUG_ASSERT(i_id == intersection_uvs.size());
        if (tm_e.template property_map<VertexIndex, glm::dvec2>("v:uv").has_value()) {
            // The edge belongs to the mesh being clipped
            intersection_uvs.emplace_back(true, h_e);
        } else {
            // The edge belongs to the mesh that is clipping
            intersection_uvs.emplace_back(false, h_f);
        }
    }

    void new_vertex_added(size_t i_id, VertexDescriptor vh, const TriangleMesh& tm) {
        const auto [is_edge, halfedge] = intersection_uvs[i_id];
        if (is_edge) {
            // The vertex was created from an edge being split
            const auto source_vertex = tm.source(halfedge);
            const auto target_vertex = tm.target(halfedge);
            // Calculate t value for the split vertex
            const auto old_source_position = convert::to_glm_point(tm.point(source_vertex));
            const auto old_target_position = convert::to_glm_point(tm.point(target_vertex));
            const auto new_position = convert::to_glm_point(tm.point(vh));
            const auto t = glm::distance(new_position, old_source_position) / glm::distance(old_target_position, old_source_position);
            // Interpolate UVs for the split vertex based on the original vertices
            const auto old_source_uv = uv_map[source_vertex];
            const auto old_target_uv = uv_map[target_vertex];
            const auto new_uv = glm::mix(old_source_uv, old_target_uv, t);
            uv_map[vh] = new_uv;
        } else {
            // The vertex was created inside a face
            // TODO:
        }
    }
};
}

SimpleMesh mesh::clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh &clip_mesh) {
    using UvMap = cgal::SurfaceMesh::Property_map<cgal::VertexIndex, glm::dvec2>;

    cgal::SurfaceMesh cgal_mesh = convert::to_cgal_mesh(mesh);
    cgal::SurfaceMesh cgal_clip_mesh = convert::to_cgal_mesh(clip_mesh);

    bool success;
    if (mesh.has_uvs()) {
        UvMap uv_map = cgal_mesh.property_map<cgal::VertexIndex, glm::dvec2>("v:uv").value();
        const auto params = CGAL::Polygon_mesh_processing::parameters::visitor(
            UvInterpolatorVisitor<cgal::SurfaceMesh, UvMap>(uv_map));
        success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh, params);
    } else {
        success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh);
    }
    if (!success) {
        throw std::runtime_error("CGAL::Polygon_mesh_processing::clip failed");
    }

    cgal_mesh.collect_garbage();
    SimpleMesh result = convert::to_simple_mesh(cgal_mesh);
    if (mesh.has_texture()) {
        result.texture = mesh.texture.value();
    }
    return result;
}

