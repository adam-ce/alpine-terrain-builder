#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>
#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "hash_utils.h"
#include "log.h"
#include "mesh/cgal.h"
#include "mesh/clip.h"
#include "mesh/convert.h"
#include "mesh/geometry.h"
#include "mesh/topology.h"
#include "mesh/validate.h"
#include "FixedVector.h"
#include "vector_utils.h"

namespace mesh {
namespace {
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
    return Intersection<T>{line.point + t * line.direction, t};
}

template <typename T>
std::optional<Intersection<T>>
compute_intersection(const radix::geometry::Edge<3, T>& edge, const radix::geometry::Plane<T>& plane) {
    const auto direction = edge[1] - edge[0];
    const auto result = compute_intersection(radix::geometry::Line<3, T>{edge[0], direction}, plane);
    if (!result) {
        return std::nullopt;
    }

    // Keep only intersections on the edge
    if (result->t < T(0) || result->t > T(1)) {
        return std::nullopt;
    }

    return result;
}

class VertexProvenance {
public:
    struct Original {
        uint32_t vertex_index;
        bool operator==(const Original &) const = default;
        auto operator<=>(const Original &) const = default;
    };

    struct OnEdge {
        glm::uvec2 edge;
        uint8_t plane_index;
        bool operator==(const OnEdge &) const = default;
        std::strong_ordering operator<=>(const OnEdge &other) const {
            if (auto c = edge.x <=> other.edge.x; c != 0) {
                return c;
            }
            if (auto c = edge.y <=> other.edge.y; c != 0) {
                return c;
            }
            return plane_index <=> other.plane_index;
        }
    };

    struct Inside {
        std::unique_ptr<std::pair<VertexProvenance, VertexProvenance>> parts;
        uint8_t plane_index;

        Inside(std::unique_ptr<std::pair<VertexProvenance, VertexProvenance>> p, uint8_t plane_index)
            : parts(std::move(p)), plane_index(plane_index) {}

        Inside(const Inside &other)
            : parts(other.parts
                        ? std::make_unique<std::pair<VertexProvenance, VertexProvenance>>(*other.parts)
                        : nullptr),
              plane_index(other.plane_index) {}

        Inside &operator=(const Inside &other) {
            if (this != &other) {
                parts = other.parts
                            ? std::make_unique<std::pair<VertexProvenance, VertexProvenance>>(*other.parts)
                            : nullptr;
                plane_index = other.plane_index;
            }
            return *this;
        }

        Inside(Inside &&) noexcept = default;
        Inside &operator=(Inside &&) noexcept = default;
        std::strong_ordering operator<=>(const Inside& other) const {
            if (auto c = plane_index <=> other.plane_index; c != 0) {
                return c;
            }

            if (!parts && !other.parts) {
                return std::strong_ordering::equal;
            }
            if (!parts) {
                return std::strong_ordering::less;
            }
            if (!other.parts) {
                return std::strong_ordering::greater;
            }

            if (auto c = parts->first <=> other.parts->first; c != 0) {
                return c;
            }
            return parts->second <=> other.parts->second;
        }
        
        bool operator==(const Inside& other) const {
            if (plane_index != other.plane_index) {
                return false;
            }
            if (static_cast<bool>(parts) != static_cast<bool>(other.parts)) {
                return false;
            }
            if (!parts) {
                return true;
            }
            return parts->first == other.parts->first &&
                parts->second == other.parts->second;
        }
    };

    VertexProvenance(const Original &v) : _v(v) {}
    VertexProvenance(const OnEdge &v) : _v(v) {}
    VertexProvenance(const Inside &v) : _v(v) {}

    bool operator==(const VertexProvenance &) const = default;
    std::strong_ordering operator<=>(const VertexProvenance &other) const {
        if (auto c = _v.index() <=> other._v.index(); c != 0) {
            return c;
        }

        return std::visit(
            [](const auto &a, const auto &b) -> std::strong_ordering {
                using A = std::decay_t<decltype(a)>;
                using B = std::decay_t<decltype(b)>;
                if constexpr (std::is_same_v<A, B>) {
                    return a <=> b;
                } else {
                    UNREACHABLE();
                }
            },
            _v, other._v);
    }

    bool is_original() const {
        return std::holds_alternative<Original>(_v);
    }

    bool is_on_edge() const {
        return std::holds_alternative<OnEdge>(_v);
    }

    bool is_inside() const {
        return std::holds_alternative<Inside>(_v);
    }

    static VertexProvenance make_original(const uint32_t vertex_index) {
        return VertexProvenance{Original{vertex_index}};
    }

    static VertexProvenance make_on_edge(const uint32_t a, const uint32_t b, const uint8_t plane_index) {
        return make_on_edge(glm::uvec2(a, b), plane_index);
    }

    static VertexProvenance make_on_edge(const glm::uvec2 &edge, const uint8_t plane_index) {
        return VertexProvenance{OnEdge{normalize_edge(edge), plane_index}};
    }

    static VertexProvenance make_inside(const VertexProvenance &a, const VertexProvenance &b, const uint8_t plane_index) {
        auto parts = (a < b)
                         ? std::make_unique<std::pair<VertexProvenance, VertexProvenance>>(a, b)
                         : std::make_unique<std::pair<VertexProvenance, VertexProvenance>>(b, a);
        return VertexProvenance{Inside{std::move(parts), plane_index}};
    }

    static VertexProvenance make_intersection(const VertexProvenance &a, const VertexProvenance &b, const uint8_t plane_index, const double t) {
        if (t == 0) {
            return a;
        }
        if (t == 1) {
            return b;
        }
        return make_intersection(a, b, plane_index);
    }

    static VertexProvenance make_intersection(
        const VertexProvenance &a,
        const VertexProvenance &b,
        uint8_t plane_index) {
        return std::visit(
            [&](const auto &av, const auto &bv) -> VertexProvenance {
                using A = std::decay_t<decltype(av)>;
                using B = std::decay_t<decltype(bv)>;

                if constexpr (std::is_same_v<A, Original> && std::is_same_v<B, Original>) {
                    if (av.vertex_index == bv.vertex_index) {
                        return a;
                    }
                    return make_on_edge(av.vertex_index, bv.vertex_index, plane_index);
                }

                else if constexpr (std::is_same_v<A, Original> && std::is_same_v<B, OnEdge>) {
                    if (point_lies_on_edge(av.vertex_index, bv.edge)) {
                        return make_on_edge(bv.edge, plane_index);
                    }
                    return make_inside(a, b, plane_index);
                }

                else if constexpr (std::is_same_v<A, OnEdge> && std::is_same_v<B, Original>) {
                    return make_intersection(b, a, plane_index);
                }

                else {
                    return make_inside(a, b, plane_index);
                }
            },
            a._v, b._v);
    }

    template <typename F>
    decltype(auto) visit(F &&f) {
        return std::visit(std::forward<F>(f), _v);
    }

    template <typename F>
    decltype(auto) visit(F &&f) const {
        return std::visit(std::forward<F>(f), _v);
    }

    struct Hash {
        size_t operator()(const VertexProvenance &v) const {
            return std::visit(*this, v._v);
        }

        size_t operator()(const Original &v) const {
            return v.vertex_index;
        }

        size_t operator()(const OnEdge &v) const {
            return hash::combine(v.edge.x, v.edge.y, v.plane_index);
        }

        size_t operator()(const Inside &v) const {
            size_t h = hash::combine(v.plane_index);
            if (v.parts) {
                hash::append(h, (*this)(v.parts->first));
                hash::append(h, (*this)(v.parts->second));
            }
            return h;
        }
    };

private:
    std::variant<Original, OnEdge, Inside> _v;

    static bool point_lies_on_edge(const uint32_t vertex_index, const glm::uvec2 &edge) {
        return vertex_index == edge.x || vertex_index == edge.y;
    }
};

} // namespace

Cow<const SimpleMesh> clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds) {
    validate(mesh);

    if (mesh.vertex_count() == 0 || mesh.face_count() == 0) {
        return Cow(SimpleMesh());
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
    std::unordered_map<VertexProvenance, uint32_t, VertexProvenance::Hash> seen_vertices(mesh.positions.size());
    auto add_intersection_vertex = [&](const VertexProvenance &provenance, const glm::dvec3 &position, const glm::dvec2 &uv) {
        const auto it = seen_vertices.find(provenance);
        if (it != seen_vertices.cend()) {
            return it->second;
        } else {
            const uint32_t vertex_index = new_positions.size();
            new_positions.push_back(position);
            if (mesh.has_uvs()) {
                new_uvs.push_back(uv);
            }
            seen_vertices.emplace(provenance, vertex_index);
            return vertex_index;
        }
    };

    // Create another simpler mapping for the vertices copied directly from the input mesh
    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    std::vector<uint32_t> position_mapping(mesh.vertex_count(), invalid_index);
    auto add_original_vertex = [&](const uint32_t original_index, const glm::dvec3 &position, const glm::dvec2 &uv) {
        uint32_t &mapped = position_mapping[original_index];
        if (mapped == invalid_index) {
            mapped = new_positions.size();
            DEBUG_ASSERT(position == mesh.positions[original_index]);
            new_positions.push_back(position);
            if (mesh.has_uvs()) {
                DEBUG_ASSERT(uv == mesh.uvs[original_index]);
                new_uvs.push_back(uv);
            }
        }
        return mapped;
    };

    // Prepare temporary storage for intermediate triangles,
    // to avoid allocating a new vector for each triangle in the slow path.
    struct TriangleAndVertices {
        std::array<VertexProvenance, 3> vertex_provenance;
        std::array<glm::dvec3, 3> positions;
        std::array<glm::dvec2, 3> uvs;
        uint8_t next_plane_to_clip; // Which planes have already clipped this triangle
    };
    FixedVector<TriangleAndVertices, 6> triangles_left_to_clip;

    // Iterate over each triangle in the mesh
    for (const glm::uvec3 &source_triangle : mesh.triangles) {
        // Get the positions for the current triangle
        const std::array<glm::dvec3, 3> source_vertices = {
            mesh.positions[source_triangle.x],
            mesh.positions[source_triangle.y],
            mesh.positions[source_triangle.z]};

        // Load triangle uvs here to avoid repeated conditionals later
        std::array<glm::dvec2, 3> source_uvs{};
        if (mesh.has_uvs()) {
            source_uvs = {
                mesh.uvs[source_triangle.x],
                mesh.uvs[source_triangle.y],
                mesh.uvs[source_triangle.z]};
        };

        // Start with a few quick checks to try to avoid the slow path
        const uint8_t in_bounds_count = std::count_if(source_vertices.begin(), source_vertices.end(), [&](const auto &vertex) {
            return bounds.contains_inclusive(vertex);
        });
        if (in_bounds_count == 0) {
            // All points are outside the bounds, but the triangle could still intersect.
            // Calculate the triangle bounds and perform an intersection check
            const glm::dvec3 triangle_min = glm::min(glm::min(source_vertices[0], source_vertices[1]), source_vertices[2]);
            const glm::dvec3 triangle_max = glm::max(glm::max(source_vertices[0], source_vertices[1]), source_vertices[2]);
            if (!radix::geometry::intersect(bounds, radix::geometry::Aabb3d{triangle_min, triangle_max})) {
                // If the triangle bounds dont intersect, the triangle can be discarded
                continue;
            }
        }
        if (in_bounds_count == 3) {
            // All vertices are in bounds, so we can directly add the vertex to the result mesh
            glm::uvec3 new_triangle;
            for (uint8_t k = 0; k < 3; k++) {
                new_triangle[k] = add_original_vertex(source_triangle[k], source_vertices[k], source_uvs[k]);
            }
            new_triangles.push_back(new_triangle);
            continue;
        }

        // Slow path: Clip triangle against all planes
        
        // Prepare vertex provenance
        const std::array<VertexProvenance, 3> source_provenance = {
            VertexProvenance::make_original(source_triangle[0]),
            VertexProvenance::make_original(source_triangle[1]),
            VertexProvenance::make_original(source_triangle[2]),
        };

        TriangleAndVertices current_triangle_and_vertices = {source_provenance, source_vertices, source_uvs, 0};
        triangles_left_to_clip.clear();

        // This outer loop iterates over the intermediate triangles potentially produced in the 2 inside 1 outside case
        // The current_triangle_and_vertices is updated inside or at the end of the loop
        while (true) {
            const auto &[vertex_provenance, vertices, uvs, next_plane_to_clip] = current_triangle_and_vertices;
            bool skip_triangle = false;

            // Clip the current triangle against all planes
            for (uint8_t plane_index = next_plane_to_clip; plane_index < planes.size(); plane_index++) {
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
                    for (uint8_t k = 0; k < 3; k++) {
                        if (vertex_inside[k]) {
                            inside_tri_index = k;
                            outside1_tri_index = (k + 1) % 3;
                            outside2_tri_index = (k + 2) % 3;
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

                    const Intersection intersection1 = compute_intersection(radix::geometry::Edge{inside_vertex, outside1_vertex}, plane).value();
                    const Intersection intersection2 = compute_intersection(radix::geometry::Edge{inside_vertex, outside2_vertex}, plane).value();

                    // Compute the uvs for the vertices and intersection points
                    const glm::dvec2 inside_uv = uvs[inside_tri_index];
                    const glm::dvec2 outside1_uv = uvs[outside1_tri_index];
                    const glm::dvec2 outside2_uv = uvs[outside2_tri_index];
                    const glm::dvec2 intersection1_uv = glm::mix(inside_uv, outside1_uv, intersection1.t);
                    const glm::dvec2 intersection2_uv = glm::mix(inside_uv, outside2_uv, intersection2.t);

                    // Compute the provenances
                    const VertexProvenance inside_prov = vertex_provenance[inside_tri_index];
                    const VertexProvenance outside1_prov = vertex_provenance[outside1_tri_index];
                    const VertexProvenance outside2_prov = vertex_provenance[outside2_tri_index];
                    const VertexProvenance intersection1_prov = VertexProvenance::make_intersection(inside_prov, outside1_prov, plane_index, intersection1.t);
                    const VertexProvenance intersection2_prov = VertexProvenance::make_intersection(inside_prov, outside2_prov, plane_index, intersection2.t);

                    // Finally create the new triangle and use it as the current triangle
                    // The intersection vertices are marked as invalid, since they have not yet been added to the output.
                    const std::array<VertexProvenance, 3> new_vertex_provenance = {inside_prov, intersection1_prov, intersection2_prov};
                    const std::array<glm::dvec3, 3> new_vertices = {inside_vertex, intersection1.point, intersection2.point};
                    const std::array<glm::dvec2, 3> new_uvs = {inside_uv, intersection1_uv, intersection2_uv};

                    if (is_empty_triangle(new_vertices)) {
                        skip_triangle = true;
                        break;
                    }

                    const uint8_t next_plane_index = static_cast<uint8_t>(plane_index + 1);
                    current_triangle_and_vertices = {new_vertex_provenance, new_vertices, new_uvs, next_plane_index};
                } else if (inside_count == 2) {
                    // Two vertices is inside the plane, cut the last one off and split the triangle.

                    // First identify the vertices
                    uint32_t outside_tri_index, inside1_tri_index, inside2_tri_index;
                    for (uint8_t k = 0; k < 3; k++) {
                        if (!vertex_inside[k]) {
                            outside_tri_index = k;
                            inside1_tri_index = (k + 1) % 3;
                            inside2_tri_index = (k + 2) % 3;
                            break;
                        }
                    }

                    // Then compute the intersections
                    const glm::dvec3 outside_vertex = vertices[outside_tri_index];
                    const glm::dvec3 inside1_vertex = vertices[inside1_tri_index];
                    const glm::dvec3 inside2_vertex = vertices[inside2_tri_index];

                    const Intersection intersection1 = compute_intersection(radix::geometry::Edge{outside_vertex, inside1_vertex}, plane).value();
                    const Intersection intersection2 = compute_intersection(radix::geometry::Edge{outside_vertex, inside2_vertex}, plane).value();

                    // Compute the Uvs for the vertices and intersection points
                    const glm::dvec2 outside_uv = uvs[outside_tri_index];
                    const glm::dvec2 inside1_uv = uvs[inside1_tri_index];
                    const glm::dvec2 inside2_uv = uvs[inside2_tri_index];
                    const glm::dvec2 intersection1_uv = glm::mix(outside_uv, inside1_uv, intersection1.t);
                    const glm::dvec2 intersection2_uv = glm::mix(outside_uv, inside2_uv, intersection2.t);

                    // Compute the provenances
                    const VertexProvenance outside_prov = vertex_provenance[outside_tri_index];
                    const VertexProvenance inside1_prov = vertex_provenance[inside1_tri_index];
                    const VertexProvenance inside2_prov = vertex_provenance[inside2_tri_index];
                    const VertexProvenance intersection1_prov = VertexProvenance::make_intersection(outside_prov, inside1_prov, plane_index, intersection1.t);
                    const VertexProvenance intersection2_prov = VertexProvenance::make_intersection(outside_prov, inside2_prov, plane_index, intersection2.t);

                    // Finally create the new triangles
                    // quad order: outside - int1 - inside1 - inside2 - int2
                    const std::array<VertexProvenance, 3> new_vertex_provenance1 = {inside1_prov, inside2_prov, intersection1_prov};
                    const std::array<VertexProvenance, 3> new_vertex_provenance2 = {inside2_prov, intersection2_prov, intersection1_prov};
                    const std::array<glm::dvec3, 3> new_vertices1 = {inside1_vertex, inside2_vertex, intersection1.point};
                    const std::array<glm::dvec3, 3> new_vertices2 = {inside2_vertex, intersection2.point, intersection1.point};
                    const std::array<glm::dvec2, 3> new_uvs1 = {inside1_uv, inside2_uv, intersection1_uv};
                    const std::array<glm::dvec2, 3> new_uvs2 = {inside2_uv, intersection2_uv, intersection1_uv};

                    const uint8_t next_plane_index = static_cast<uint8_t>(plane_index + 1);
                    const TriangleAndVertices new_triangle1{new_vertex_provenance1, new_vertices1, new_uvs1, next_plane_index};
                    const TriangleAndVertices new_triangle2{new_vertex_provenance2, new_vertices2, new_uvs2, next_plane_index};

                    // Set the first triangle as the current one
                    // Push the second triangle to the list of triangles left to clip
                    const bool empty1 = is_empty_triangle(new_triangle1.positions);
                    const bool empty2 = is_empty_triangle(new_triangle2.positions);
                    if (empty2) {
                        if (empty1) {
                            skip_triangle = true;
                            break;
                        } else {
                            current_triangle_and_vertices = new_triangle1;
                        }
                    }
                    else {
                        if (empty1) {
                            current_triangle_and_vertices = new_triangle2;
                        } else {
                            current_triangle_and_vertices = new_triangle1;
                            triangles_left_to_clip.push_back(new_triangle2);
                        }
                    }
                } else {
                    UNREACHABLE();
                }
            }

            if (skip_triangle) {
                break;
            }

            // If we reached here we have a clipped triangle that was not discarded
            // However since it may contain new vertices not in the original mesh
            // we need to add them to the output mesh while avoiding duplicates
            glm::uvec3 indices;
            for (uint8_t k = 0; k < 3; k++) {
                const VertexProvenance &provenance = vertex_provenance[k];
                const auto &position = vertices[k];
                const auto &uv = uvs[k];
                indices[k] = provenance.visit([&](const auto &v) -> uint32_t {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, VertexProvenance::Original>) {
                        // We have a vertex in the original mesh -> look up in the boolean vector
                        return add_original_vertex(v.vertex_index, position, uv);
                    } else {
                        // We have a vertex not in the original mesh -> look up in the spatial hash map
                        return add_intersection_vertex(v, position, uv);
                    }
                });
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

    SimpleMesh clipped_mesh(new_triangles, new_positions, new_uvs, mesh.texture);
    validate(clipped_mesh);
    return Cow(std::move(clipped_mesh));
}

namespace {
template <typename TriangleMesh>
struct HasIntersectionsVisitor : public CGAL::Polygon_mesh_processing::Corefinement::Default_visitor<TriangleMesh> {
    using HalfedgeDescriptor = typename boost::graph_traits<TriangleMesh>::halfedge_descriptor;

    bool has_intersections = false;

    // called when a new intersection point is detected.
    // The intersection is detected using a face of tm_f and an edge of tm_e. 
    void intersection_point_detected(
        size_t /*i_id*/, int /*sdim*/,
        HalfedgeDescriptor /*h_e*/, HalfedgeDescriptor /*h_f*/, 
        const TriangleMesh& /*tm_e*/, const TriangleMesh& /*tm_f*/, bool /*is_target_coplanar*/, bool /*is_source_coplanar*/) {
        this->has_intersections = true;
    }
};
} // namespace

Cow<const SimpleMesh> clip_on_bounds_and_cap(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds, const bool remesh_planar_patches) {
    ASSERT(!mesh.has_uvs());
    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);

    const CGAL::Iso_cuboid_3<cgal::Kernel> cgal_bounds(
        convert::to_cgal_point<cgal::Kernel>(bounds.min),
        convert::to_cgal_point<cgal::Kernel>(bounds.max));
    HasIntersectionsVisitor<cgal::Mesh> visitor;
    const auto clip_params = CGAL::Polygon_mesh_processing::parameters::clip_volume(true)
        .visitor(visitor);
    cgal::Mesh result_cgal_mesh;
    const bool success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_bounds, clip_params);
    if (!success) {
        throw std::runtime_error("CGAL::Polygon_mesh_processing::clip failed");
    }
    if (cgal_mesh.number_of_faces() == 0) {
        return Cow(SimpleMesh());
    }
    if (!visitor.has_intersections && cgal_mesh.number_of_faces() == mesh.face_count()) {
        return Cow(mesh);
    }

    cgal_mesh.collect_garbage();

    if (!remesh_planar_patches) {
        return Cow(convert::to_simple_mesh(cgal_mesh));
    }
    
    cgal::Mesh remeshed_cgal_mesh;
    const auto remesh_params = CGAL::Polygon_mesh_processing::parameters::cosine_of_maximum_angle(0.999);
    CGAL::Polygon_mesh_processing::remesh_planar_patches(cgal_mesh, remeshed_cgal_mesh, remesh_params);
    remeshed_cgal_mesh.collect_garbage();
    return Cow(convert::to_simple_mesh(remeshed_cgal_mesh));
}

namespace {
template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> compute_barycentric(
    const glm::vec<n_dims, T> &point,
    const glm::vec<n_dims, T> &a,
    const glm::vec<n_dims, T> &b,
    const glm::vec<n_dims, T> &c
) {
    using Vec = glm::vec<n_dims, T>;

    const Vec v0 = b - a;
    const Vec v1 = c - a;
    const Vec v2 = point - a;

    const T d00 = glm::dot(v0, v0);
    const T d01 = glm::dot(v0, v1);
    const T d11 = glm::dot(v1, v1);
    const T d20 = glm::dot(v2, v0);
    const T d21 = glm::dot(v2, v1);

    const T denom = d00 * d11 - d01 * d01;
    ASSERT(denom != 0);
    const T inv_denom = 1 / denom;

    const T v = (d11 * d20 - d01 * d21) * inv_denom;
    const T w = (d00 * d21 - d01 * d20) * inv_denom;
    const T u = 1 - v - w;

    return Vec(u, v, w);
}
template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> compute_barycentric(const glm::vec<n_dims, T> &point, const std::array<glm::vec<n_dims, T>, 3>& triangle) {
    return compute_barycentric(point, triangle[0], triangle[1], triangle[2]);
}

template <typename TriangleMesh, typename UvMap>
struct UvInterpolatorVisitor : public CGAL::Polygon_mesh_processing::Corefinement::Default_visitor<TriangleMesh> {
    using HalfedgeDescriptor = typename boost::graph_traits<TriangleMesh>::halfedge_descriptor;
    using VertexDescriptor = typename boost::graph_traits<TriangleMesh>::vertex_descriptor;
    using VertexIndex = typename TriangleMesh::Vertex_index;

    UvMap& uv_map;
    TriangleMesh &mesh;

    struct Vertex {
        glm::dvec3 position;
        glm::dvec2 uv;
    };

    UvInterpolatorVisitor(UvMap& uv_map, TriangleMesh& mesh) : uv_map(uv_map), mesh(mesh) {}

    struct EdgeIntersection {
        Vertex source;
        Vertex target;
    };

    struct FaceIntersection {
        std::array<Vertex, 3> vertices;
    };
    std::vector<std::variant<EdgeIntersection, FaceIntersection>> intersections;

    // called when a new intersection point is detected.
    // The intersection is detected using a face of tm_f and an edge of tm_e. 
    void intersection_point_detected(
        size_t i_id, int /*sdim*/,
        HalfedgeDescriptor h_e, HalfedgeDescriptor h_f, 
        const TriangleMesh& tm_e, const TriangleMesh& tm_f, bool /*is_target_coplanar*/, bool /*is_source_coplanar*/) {
        DEBUG_ASSERT(i_id == intersections.size());
        if (&mesh == &tm_e) {
            // The edge belongs to the mesh being clipped
            const auto source_vertex = mesh.source(h_e);
            const auto target_vertex = mesh.target(h_e);
            const auto source_position = convert::to_glm_point(mesh.point(source_vertex));
            const auto target_position = convert::to_glm_point(mesh.point(target_vertex));
            const auto source_uv = uv_map[source_vertex];
            const auto target_uv = uv_map[target_vertex];
            intersections.emplace_back(EdgeIntersection {
                Vertex(source_position, source_uv),
                Vertex(target_position, target_uv)
            });
        } else {
            DEBUG_ASSERT(&mesh == &tm_f);
            // The face belongs to the mesh that is being clipping
            std::array<Vertex, 3> triangle;
            unsigned int i = 0;
            for (const cgal::VertexIndex vertex_index : CGAL::vertices_around_face(h_f, tm_f)) {
                ASSERT(i < triangle.size());
                const auto position = convert::to_glm_point(mesh.point(vertex_index));
                const auto uv = uv_map[vertex_index];
                triangle[i] = Vertex(position, uv);
                i++;
            }

            intersections.emplace_back(FaceIntersection {
                triangle
            });
        }
    }

    void new_vertex_added(size_t i_id, VertexDescriptor vh, const TriangleMesh& tm) {
        DEBUG_ASSERT(i_id < intersections.size());
        if (&tm != &mesh) {
            return;
        }

        const glm::dvec3 new_position = convert::to_glm_point(mesh.point(vh));
        const glm::dvec2 new_uv = std::visit([&](const auto& intersection) {
            using Intersection = std::decay_t<decltype(intersection)>;
            if constexpr (std::is_same_v<Intersection, EdgeIntersection>) {
                const double edge_length = glm::distance(intersection.source.position, intersection.target.position);
                const double t = glm::distance(new_position, intersection.source.position) / edge_length;
                return glm::mix(intersection.source.uv, intersection.target.uv, t);
            } else {
                const glm::dvec3 barycentric = compute_barycentric(
                    new_position,
                    intersection.vertices[0].position,
                    intersection.vertices[1].position,
                    intersection.vertices[2].position
                );
                // return glm::dvec2(1, 1);
                return barycentric[0] * intersection.vertices[0].uv
                    + barycentric[1] * intersection.vertices[1].uv
                    + barycentric[2] * intersection.vertices[2].uv;
            }
        }, intersections[i_id]);
        uv_map[vh] = new_uv;
    }
};
} // namespace

Cow<const SimpleMesh> clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh &clip_mesh, const bool keep_inside) {
    // short circuit the empty mesh case
    // this is a common case since we clip the mask in the terrainmerger on the octree node bounds
    // which often results in empty masks
    if (clip_mesh.face_count() == 0) {
        if (keep_inside) {
            return Cow<const SimpleMesh>::from_owned(SimpleMesh());
        } else {
            return Cow<const SimpleMesh>::from_ref(mesh);
        }
    }
    
    // If we want to keep everything outside the clip_mesh we simply invert it and recurse
    if (!keep_inside) {
        SimpleMesh clip_mesh_inv(clip_mesh.triangles, clip_mesh.positions);
        flip_orientation(clip_mesh_inv);
        return mesh::clip_on_mesh(mesh, clip_mesh_inv, !keep_inside);
    }
    
    // Convert both meshes to cgal and use their clipping logic.
    using UvMap = cgal::Mesh::Property_map<cgal::VertexIndex, glm::dvec2>;

    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    cgal::Mesh cgal_clip_mesh = convert::to_cgal_mesh(clip_mesh);

    bool success;
    bool has_intersections;
    if (mesh.has_uvs()) {
        UvMap uv_map = cgal_mesh.property_map<cgal::VertexIndex, glm::dvec2>("v:uv").value();
        UvInterpolatorVisitor<cgal::Mesh, UvMap> visitor(uv_map, cgal_mesh);
        const auto params = CGAL::Polygon_mesh_processing::parameters::visitor(visitor);
        success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh, params);
        has_intersections = !visitor.intersections.empty();
    } else {
        HasIntersectionsVisitor<cgal::Mesh> visitor;
        const auto params = CGAL::Polygon_mesh_processing::parameters::visitor(visitor);
        success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh, params);
        has_intersections = visitor.has_intersections;
    }
    if (!success) {
        throw std::runtime_error("CGAL::Polygon_mesh_processing::clip failed");
    }
    if (cgal_mesh.number_of_faces() == 0) {
        return Cow(SimpleMesh());
    }
    if (!has_intersections && cgal_mesh.number_of_faces() == mesh.face_count()) {
        return Cow(mesh);
    }
    
    cgal_mesh.collect_garbage();
    SimpleMesh result = convert::to_simple_mesh(cgal_mesh);
    if (mesh.has_texture()) {
        result.texture = mesh.texture.value();
    }
    return Cow(std::move(result));
}

}
