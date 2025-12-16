#pragma once

#include <glm/gtx/norm.hpp>

#include "HybridVector.h"

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
std::vector<size_t> find_isolated_vertices(const SimpleMesh_<n_dims, T> &mesh) {
    std::vector<bool> connected;
    connected.resize(mesh.vertex_count());
    std::fill(connected.begin(), connected.end(), false);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            connected[triangle[k]] = true;
        }
    }

    std::vector<size_t> isolated;
    for (size_t i = 0; i < mesh.vertex_count(); i++) {
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
    for (size_t i = 0; i < triangles.size(); i++) {
        glm::uvec3 triangle = triangles[i];
        if (normalize) {
            normalize_triangle_inplace(triangle, true);
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
std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh) {
    std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> boundary;
    find_boundary_edges(mesh, boundary);
    return boundary;
}
template <glm::length_t n_dims, typename T>
void find_boundary_edges(const SimpleMesh_<n_dims, T> &mesh, std::unordered_set<typename SimpleMesh_<n_dims, T>::Edge> &boundary) {
    for_each_edge(mesh, [&](const glm::uvec2 &edge, const size_t /*triangle_index*/) {
        auto it = boundary.find(glm::uvec2(edge.y, edge.x));
        if (it != boundary.end()) {
            // Edge already there -> shared egde -> remove it
            boundary.erase(it);
        } else {
            // Edge not present -> add it (but in correct order)
            boundary.insert(edge);
        }
    }, /* normalize */ false);
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
std::vector<size_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, bool ignore_orientation) {
    return find_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
std::vector<size_t> find_duplicate_triangles(
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
size_t identify_triangle_to_remove(
    const size_t a, const size_t b,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    std::vector<size_t> &neighbourhood,
    const std::unordered_map<glm::uvec2, HybridVector<size_t, 2>> &edge_to_triangle
) {
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

        const HybridVector<size_t, 2> &adjacent_triangles = it->second;
        for (const size_t triangle_index : adjacent_triangles) {
            neighbourhood.push_back(triangle_index);
        }
    }

    // Remove duplicates
    std::sort(neighbourhood.begin(), neighbourhood.end());
    neighbourhood.erase(std::unique(neighbourhood.begin(), neighbourhood.end()), neighbourhood.end());

    // Calculate dominant normal
    glm::dvec3 average_normal(0.0);
    for (const size_t triangle_index : neighbourhood) {
        const glm::uvec3& triangle = triangles[triangle_index];
        const glm::dvec3 scaled_normal = compute_normal(triangle, positions, false);
        average_normal += scaled_normal;
    }
    average_normal = glm::normalize(average_normal);

    // Identify triangle with most deviating normal
    const glm::dvec3 normal_a = compute_normal(triangle_a, positions);
    const glm::dvec3 normal_b = compute_normal(triangle_a, positions);
    size_t triangle_to_remove = a;
    if (glm::dot(normal_b, average_normal) < glm::dot(normal_a, average_normal)) {
        triangle_to_remove = b;
    }
    return triangle_to_remove;
}
}

template <typename T>
std::vector<size_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions) {
    const auto edge_to_triangle = create_edge_to_triangle_index_mapping_non_manifold(triangles);

    std::vector<size_t> neighbourhood;
    std::vector<size_t> triangles_to_remove;
    for (const auto &entry : edge_to_triangle) {
        const HybridVector<size_t, 2>& triangle_indices = entry.second;
        const size_t num_triangles = triangle_indices.size();
        DEBUG_ASSERT(triangle_indices.size() != 0);

        // Find duplicates
        for (size_t i = 0; i < num_triangles; i++) {
            for (size_t j = i + 1; j < num_triangles; j++) {
                const size_t a = triangle_indices[i];
                const size_t b = triangle_indices[j];
                const bool are_equal = compare_equality_triangles_ignore_orientation(
                    triangles[a], triangles[b]);
                if (are_equal) {
                    // Determine which triangle to remove
                    const size_t triangle_to_remove = identify_triangle_to_remove(
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
    const std::vector<size_t> triangles_to_remove = find_duplicate_triangles(triangles, positions, ignore_orientation);

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
    const std::span<const glm::vec<3, T>> positions) {
    remove_duplicate_triangles<T>(triangles, positions, true);
}

template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> &mesh) {
    make_manifold(mesh.triangles, mesh.positions, mesh.uvs);
}

template <glm::length_t n_dims, typename Position, typename Uv>
void make_manifold(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>>& positions,
    std::vector<glm::vec<2, Uv>>& uvs
) {
    const auto edge_to_triangle = create_edge_to_triangle_index_mapping_non_manifold(triangles);

    for (const auto &entry : edge_to_triangle) {
        const glm::uvec2& edge = entry.first;
        const std::span<const size_t> triangle_indices = entry.second;
        const size_t num_triangles = triangle_indices.size();
        DEBUG_ASSERT(num_triangles != 0);

        if (num_triangles <= 2) {
            // Manifold edge
            continue;
        }

        // Non-manifold edge
        for (size_t i = 2; i < num_triangles; i++) {
            const size_t triangle_index = triangle_indices[i];
            glm::uvec3& triangle = triangles[triangle_index];
            for (size_t k=0; k<2; k++) {
                const size_t new_vertex_index = positions.size();
                positions.push_back(positions[edge[k]]);
                if (uvs.size() > 0) {
                    uvs.push_back(uvs[edge[k]]);
                }

                for (size_t v = 0; v < 3; v++) {
                    if (triangle[v] == edge[k]) {
                        triangle[v] = static_cast<uint32_t>(new_vertex_index);
                    }
                }
            }
        }
    }

#if DEBUG
    const std::vector<glm::uvec3> triangles_copy(triangles.begin(), triangles.end());
    const mesh::Simple_<n_dims, T> mesh{triangles_copy, positions, uvs}
    mesh::validate(mesh);
#endif
}
