#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <glm/common.hpp>

#include "TriangleContainer.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/normalize.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::Simple_<n_dims, T> &mesh) {
    return get_edges(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::View_<n_dims, T> &mesh) {
    return get_edges(mesh.triangles);
}
template <TriangleContainer Triangles>
std::unordered_set<glm::uvec2> get_edges(const Triangles &triangles) {
    std::unordered_set<glm::uvec2> edges;
    for_each_halfedge(triangles, [&](const glm::uvec2 &edge) { edges.insert(edge); }, true);
    return edges;
}

template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> get_halfedges(const mesh::Simple_<n_dims, T> &mesh) {
    return get_halfedges(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> get_halfedges(const mesh::View_<n_dims, T> &mesh) {
    return get_halfedges(mesh.triangles);
}
template <TriangleContainer Triangles>
std::vector<glm::uvec2> get_halfedges(const Triangles &triangles) {
    std::vector<glm::uvec2> edges;
    edges.reserve(3 * triangles.size());
    for_each_halfedge(triangles, [&](const glm::uvec2 &edge) { edges.push_back(edge); }, false);
    return edges;
}

template <glm::length_t n_dims, typename T>
uint32_t compute_edge_count(const mesh::Simple_<n_dims, T> &mesh) {
    return compute_edge_count(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
uint32_t compute_edge_count(const mesh::View_<n_dims, T> &mesh) {
    return compute_edge_count(mesh.triangles);
}
template <TriangleContainer Triangles>
uint32_t compute_edge_count(const Triangles &triangles) {
    return get_edges(triangles).size();
}

template <glm::length_t n_dims, typename T>
uint32_t compute_halfedge_count(const mesh::Simple_<n_dims, T> &mesh) {
    return compute_halfedge_count(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
uint32_t compute_halfedge_count(const mesh::View_<n_dims, T> &mesh) {
    return compute_halfedge_count(mesh.triangles);
}
template <TriangleContainer Triangles>
uint32_t compute_halfedge_count(const Triangles &triangles) {
    uint32_t count = 0;
    for_each_halfedge(triangles, [&](const glm::uvec2 &) { count++; });
    return count;
}

template <glm::length_t n_dims, typename T, typename F>
void for_each_halfedge(const mesh::Simple_<n_dims, T> &mesh, F &&func, const bool normalize) {
    for_each_halfedge(mesh.triangles, std::forward<F>(func), normalize);
}
template <glm::length_t n_dims, typename T, typename F>
void for_each_halfedge(const mesh::View_<n_dims, T> &mesh, F &&func, const bool normalize) {
    for_each_halfedge(mesh.triangles, std::forward<F>(func), normalize);
}
template <TriangleContainer Triangles, typename F>
void for_each_halfedge(const Triangles &triangles, F &&func, const bool normalize) {
    if constexpr (std::invocable<F &, glm::uvec2>) {
        for_each_halfedge(
            triangles,
            [&](const glm::uvec2 edge, const uint32_t) {
                return func(edge);
            },
            normalize);
    } else {
        static_assert(std::invocable<F &, glm::uvec2, uint32_t>);
        using Result = std::invoke_result_t<F &, glm::uvec2, uint32_t>;
        if constexpr (std::same_as<Result, void>) {
            for_each_halfedge(
                triangles,
                [&](const glm::uvec2 edge, const uint32_t triangle_index) {
                    func(edge, triangle_index);
                    return true;
                },
                normalize);
        } else {
            static_assert(std::is_same_v<Result, bool>);
            for (uint32_t i = 0; i < triangles.size(); i++) {
                glm::uvec3 triangle = triangles[i];
                if (normalize) {
                    normalize_triangle_inplace(triangle, false);
                }

                bool result;
                result = func(glm::uvec2(triangle[0], triangle[1]), i);
                if (!result) {
                    return;
                }
                result = func(glm::uvec2(triangle[1], triangle[2]), i);
                if (!result) {
                    return;
                }
                if (normalize) {
                    result = func(glm::uvec2(triangle[0], triangle[2]), i);
                } else {
                    result = func(glm::uvec2(triangle[2], triangle[0]), i);
                }
                if (!result) {
                    return;
                }
            }
        }
    }
}

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F &&func) {
    for_each_edge(mesh.triangles, std::forward<F>(func));
}
template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::View_<n_dims, T> &mesh, F &&func) {
    for_each_edge(mesh.triangles, std::forward<F>(func));
}
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func) {
    static_assert(std::invocable<F &, glm::uvec2>);
    const auto edges = get_edges(triangles);
    for (const glm::uvec2 &edge : edges) {
        func(edge);
    }
}

}
