#include "mesh/connected_components.h"

namespace {
template <typename MeshRange>
auto calculate_bounds_range(const MeshRange &meshes) {
    using Mesh = std::unwrap_reference_t<std::ranges::range_value_t<MeshRange>>;
    using Vec = std::remove_cvref_t<decltype(std::declval<Mesh>().positions[0])>;
    using T = typename Vec::value_type;
    constexpr glm::length_t n_dims = Vec::length();

    radix::geometry::Aabb<n_dims, T> bounds;
    constexpr T inf = std::numeric_limits<T>::infinity();
    bounds.min = Vec(+inf);
    bounds.max = Vec(-inf);

    for (const auto &mesh_ref : meshes) {
        const Mesh &mesh = mesh_ref;
        for (const auto &position : mesh.positions) {
            bounds.expand_by(position);
        }
    }

    return bounds;
}
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh) {
    return calculate_bounds_range(std::array{std::cref(mesh)});
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes) {
    return calculate_bounds_range(meshes);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes) {
    return calculate_bounds_range(meshes);
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> find_isolated_vertices(const SimpleMesh_<n_dims, T> &mesh) {
    std::vector<bool> connected;
    connected.resize(mesh.vertex_count());
    std::fill(connected.begin(), connected.end(), false);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            connected[triangle[k]] = true;
        }
    }

    std::vector<uint32_t> isolated;
    for (uint32_t i = 0; i < mesh.vertex_count(); i++) {
        if (!connected[i]) {
            isolated.push_back(i);
        }
    }

    return isolated;
}

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(SimpleMesh_<n_dims, T> &mesh) {
    sort_and_normalize_triangles(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
void flip_orientation(SimpleMesh_<n_dims, T> &mesh) {
    flip_triangle_orientations(mesh.triangles);
}

template <glm::length_t n_dims, typename T, typename Func>
std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> get_edges(const SimpleMesh_<n_dims, T> &mesh) {
    using Edge = typename SimpleMesh_<n_dims, T>::Edge;
    std::unordered_set<Edge> edges;
    for (const auto& triangle_ref : mesh.triangles) {
        glm::uvec3 triangle = triangle_ref;
        std::sort(&triangle.x, &triangle.z + 1);

        const std::array<glm::uvec2, 3> face_edges{glm::uvec2(triangle.x, triangle.y),
                                              glm::uvec2(triangle.y, triangle.z),
                                              glm::uvec2(triangle.x, triangle.z)};
        for (const glm::uvec2 edge : face_edges) {
            edges.insert(edge);
        }
    }
    return edges;
}

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F&& func, const bool normalize) {
    for_each_edge(mesh.triangles, std::forward<F>(func), normalize);
}
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func, const bool normalize) {
    for (uint32_t i = 0; i < triangles.size(); i++) {
        glm::uvec3 triangle = triangles[i];
        if (normalize) {
            normalize_triangle_inplace(triangle, false);
        }

        std::forward<F>(func)(glm::uvec2(triangle[0], triangle[1]), i);
        std::forward<F>(func)(glm::uvec2(triangle[1], triangle[2]), i);
        if (normalize) {
            std::forward<F>(func)(glm::uvec2(triangle[0], triangle[2]), i);
        } else {
            std::forward<F>(func)(glm::uvec2(triangle[2], triangle[0]), i);
        }
    }
}


template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh) {
    return find_boundary_edges(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_triangles(const SimpleMesh_<n_dims, T> &mesh) {
    return find_boundary_triangles(mesh.triangles);
}


template <typename T>
WindingOrder get_winding_order(const glm::vec<2, T> &a,
                               const glm::vec<2, T> &b,
                               const glm::vec<2, T> &c) {
    const T signed_area2 =
        (b.x - a.x) * (c.y - a.y) -
        (c.x - a.x) * (b.y - a.y);

    if (signed_area2 > T(0)) {
        return WindingOrder::CounterClockwise;
    }
    if (signed_area2 < T(0)) {
        return WindingOrder::Clockwise;
    }
    return WindingOrder::Degenerate;
}

template <typename T>
WindingOrder get_winding_order(const glm::uvec3 &triangle,
                               const std::span<const glm::vec<2, T>> positions)
{
    const auto &a = positions[triangle[0]];
    const auto &b = positions[triangle[1]];
    const auto &c = positions[triangle[2]];
    return get_winding_order<T>(a, b, c);
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                            const glm::vec<3, T> &b,
                            const glm::vec<3, T> &c,
                            const bool normalize) {
    const glm::vec<3, T> n = glm::cross(b - a, c - a);
    if (normalize && glm::length2(n) > T(0)) {
        return glm::normalize(n);
    }
    return n;
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                            const std::span<const glm::vec<3, T>> positions,
                            const bool normalize) {
    const auto &a = positions[triangle[0]];
    const auto &b = positions[triangle[1]];
    const auto &c = positions[triangle[2]];
    return compute_normal(a, b, c, normalize);
}

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, bool ignore_orientation) {
    return find_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    bool ignore_orientation) {
    if (ignore_orientation) {
        return find_duplicate_triangles_ignore_orientation(triangles, positions);
    } else {
        return find_duplicate_triangles_consider_orientation(triangles);
    }
}
namespace {
template <typename T>
uint32_t identify_triangle_to_remove(
    const uint32_t a, const uint32_t b,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    std::vector<uint32_t> &neighbourhood,
    const std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> &edge_to_triangle) {
    const glm::uvec3 triangle_a = triangles[a];
    // const glm::uvec3 triangle_b = triangles[b];

    // Gather neighbourhood
    neighbourhood.clear();
    for (const glm::uvec2 edge : {
             glm::uvec2(triangle_a.x, triangle_a.y),
             glm::uvec2(triangle_a.y, triangle_a.z),
             glm::uvec2(triangle_a.z, triangle_a.x)}) {
        const auto it = edge_to_triangle.find(normalize_edge(edge));
        DEBUG_ASSERT(it != edge_to_triangle.end());

        const HybridVector<uint32_t, 2> &adjacent_triangles = it->second;
        for (const uint32_t triangle_index : adjacent_triangles) {
            neighbourhood.push_back(triangle_index);
        }
    }

    // Remove duplicates
    std::sort(neighbourhood.begin(), neighbourhood.end());
    neighbourhood.erase(std::unique(neighbourhood.begin(), neighbourhood.end()), neighbourhood.end());

    // Calculate dominant normal
    glm::dvec3 average_normal(0.0);
    for (const uint32_t triangle_index : neighbourhood) {
        const glm::uvec3& triangle = triangles[triangle_index];
        const glm::dvec3 scaled_normal = compute_normal(triangle, positions, false);
        average_normal += scaled_normal;
    }
    average_normal = glm::normalize(average_normal);

    // Identify triangle with most deviating normal
    const glm::dvec3 normal_a = compute_normal(triangle_a, positions);
    const glm::dvec3 normal_b = compute_normal(triangle_a, positions);
    uint32_t triangle_to_remove = a;
    if (glm::dot(normal_b, average_normal) < glm::dot(normal_a, average_normal)) {
        triangle_to_remove = b;
    }
    return triangle_to_remove;
}
}

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions) {
    const auto edge_to_triangle = create_edge_to_triangle_index_mapping_non_manifold(triangles);

    std::vector<uint32_t> neighbourhood;
    std::vector<uint32_t> triangles_to_remove;
    for (const auto &[_, triangle_indices] : edge_to_triangle) {
        const uint32_t num_triangles = triangle_indices.size();
        DEBUG_ASSERT(triangle_indices.size() != 0);

        // Find duplicates
        for (uint32_t i = 0; i < num_triangles; i++) {
            for (uint32_t j = i + 1; j < num_triangles; j++) {
                const uint32_t a = triangle_indices[i];
                const uint32_t b = triangle_indices[j];
                const bool are_equal = compare_equality_triangles_ignore_orientation(
                    triangles[a], triangles[b]);
                if (are_equal) {
                    // Determine which triangle to remove
                    const uint32_t triangle_to_remove = identify_triangle_to_remove(
                        a, b,
                        triangles,
                        positions,
                        neighbourhood,
                        edge_to_triangle);
                    triangles_to_remove.push_back(triangle_to_remove);
                    break;
                }
            }
        }
    }

    // Remove duplicates from list
    std::sort(triangles_to_remove.begin(), triangles_to_remove.end());
    triangles_to_remove.erase(std::unique(triangles_to_remove.begin(), triangles_to_remove.end()), triangles_to_remove.end());

    return triangles_to_remove;
}

template <typename T>
void remove_duplicate_triangles(mesh::Simple_<3, T> &mesh, bool ignore_orientation) {
    remove_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions,
    bool ignore_orientation) {
    const std::vector<uint32_t> triangles_to_remove = find_duplicate_triangles(triangles, positions, ignore_orientation);

    // Ensure indices are sorted in ascending order
    DEBUG_ASSERT(std::is_sorted(triangles_to_remove.begin(), triangles_to_remove.end()) && "triangles_to_remove must be sorted!");

    // Remove duplicates in reverse order
    for (auto it = triangles_to_remove.rbegin(); it != triangles_to_remove.rend(); ++it) {
        triangles.erase(triangles.begin() + *it);
    }
}
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    bool ignore_orientation) {
    remove_duplicate_triangles<T>(triangles, std::span<const glm::vec<3, T>>(positions), ignore_orientation);
}
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions) {
    remove_duplicate_triangles<T>(triangles, positions, true);
}
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> & positions) {
    remove_duplicate_triangles<T>(triangles, std::span<const glm::vec<3, T>>(positions));
}

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_edges(mesh::Simple_<n_dims, T> &mesh) {
    duplicate_non_manifold_edges(mesh.triangles, mesh.positions, mesh.uvs);
}

template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions
) {
    std::vector<glm::vec<2, double>> uvs; // empty uvs
    duplicate_non_manifold_edges(triangles, positions, uvs);
}

template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs
) {
    std::unordered_map<glm::uvec2, uint32_t> triangles_per_edge;

    auto duplicate_vertex = [&](const uint32_t old_vertex_index) {
        const uint32_t new_vertex_index = positions.size();
        positions.push_back(positions[old_vertex_index]);
        if (uvs.size() > 0) {
            uvs.push_back(uvs[old_vertex_index]);
        }
        return new_vertex_index;
    };

    for (glm::uvec3& triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            const glm::vec<2, uint8_t> edge_indices(k, (k + 1) % 3);
            const glm::uvec2 edge(triangle[edge_indices[0]], triangle[edge_indices[1]]);
            uint32_t& triangle_count = triangles_per_edge[normalize_edge(edge)];
            if (triangle_count < 2) {
                triangle_count++;
            } else {
                // edge already has two triangles -> duplicate edge vertices
                for (uint8_t i = 0; i < 2; i++) {
                    const uint32_t vertex_index = edge[i];
                    const uint32_t duplicate_vertex_index = duplicate_vertex(vertex_index);
                    triangle[edge_indices[i]] = duplicate_vertex_index;
                }
            }
        }
    }

#ifndef NDEBUG
    const std::vector<glm::uvec3> triangles_copy(triangles.begin(), triangles.end());
    const mesh::Simple_<n_dims, Position> mesh(triangles_copy, positions);
    DEBUG_ASSERT(find_non_manifold_edges(mesh).empty());
#endif
}

template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> & mesh) {
    make_manifold(mesh.triangles, mesh.positions, mesh.uvs);
}
template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3>& triangles,
    std::vector<glm::vec<n_dims, Position>> & positions) {
    std::vector<glm::vec<2, double>> uvs; // empty uvs
    make_manifold(triangles, positions, uvs);
}
template <glm::length_t n_dims, typename Position, typename Uv>
void make_manifold(
    std::vector<glm::uvec3>& triangles,
    std::vector<glm::vec<n_dims, Position>> & positions,
    std::vector<glm::vec<2, Uv>> & uvs) {
    remove_degenerate_triangles(triangles);
    remove_duplicate_triangles_ignore_orientation(triangles, positions);
    duplicate_non_manifold_edges(triangles, positions, uvs);
}


template <glm::length_t n_dims, typename T>
bool is_manifold(const SimpleMesh_<n_dims, T> &mesh) {
    const auto edge_to_faces = create_edge_to_triangle_index_mapping_non_manifold2(mesh);

    for (const auto &[_edge, faces] : edge_to_faces) {
        if (faces.size() > 2u) {
            return false;
        }
    }

    return true;
}

template <glm::length_t n_dims, typename T>
bool is_single_component(const SimpleMesh_<n_dims, T> &mesh) {
    if (mesh.triangles.empty()) {
        return true;
    }

    const mesh::ComponentsIndex components = find_connected_components(SimpleMesh(mesh));
    return components.component_count == 1u;
}
