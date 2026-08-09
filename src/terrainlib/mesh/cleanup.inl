#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <ranges>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/geometry.h"
#include "mesh/normalize.h"
#include "mesh/connectivity/adjacency.h"
#include "mesh/connectivity/triangle_compare.h"
#include "vector_utils.h"
#include "HybridVector.h"

namespace mesh {

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, bool ignore_orientation) {
    return find_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::View_<3, T> &mesh, bool ignore_orientation) {
    return find_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation) {
    return find_duplicate_triangles(std::span(triangles), std::span(positions), ignore_orientation);
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

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::Simple_<3, T> &mesh) {
    return find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::View_<3, T> &mesh) {
    return find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions) {
    return find_duplicate_triangles_ignore_orientation(std::span(triangles), std::span(positions));
}

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::Simple_<3, T> &mesh) {
    return find_duplicate_triangles_consider_orientation(mesh.triangles);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::View_<3, T> &mesh) {
    return find_duplicate_triangles_consider_orientation(mesh.triangles);
}
inline std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::vector<glm::uvec3> &triangles) {
    return find_duplicate_triangles_consider_orientation(std::span(triangles));
}

namespace detail {
template <typename T>
inline uint32_t identify_triangle_to_remove(
    const uint32_t a, const uint32_t b,
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    std::vector<uint32_t> &neighbourhood,
    const std::unordered_map<glm::uvec2, HybridVector<uint32_t, 2>> &edge_to_triangle) {
    const glm::uvec3 triangle_a = triangles[a];
    const glm::uvec3 triangle_b = triangles[b];

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
    dedup_by_sort(neighbourhood);

    // Calculate dominant normal
    glm::dvec3 average_normal(0.0);
    for (const uint32_t triangle_index : neighbourhood) {
        const glm::uvec3 &triangle = triangles[triangle_index];
        const glm::dvec3 scaled_normal = geometry::compute_normal(triangle, positions, false);
        average_normal += scaled_normal;
    }
    average_normal = glm::normalize(average_normal);

    // Identify triangle with most deviating normal
    const glm::dvec3 normal_a = geometry::compute_normal(triangle_a, positions);
    const glm::dvec3 normal_b = geometry::compute_normal(triangle_b, positions);
    uint32_t triangle_to_remove = a;
    if (glm::dot(normal_b, average_normal) <= glm::dot(normal_a, average_normal)) {
        triangle_to_remove = b;
    }
    return triangle_to_remove;
}
} // namespace

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions) {
    const auto edge_to_triangle = create_edge_to_triangle_mapping_non_manifold(triangles);

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
                const bool are_equal = compare_equality_triangles_ignore_orientation(triangles[a], triangles[b]);
                if (are_equal) {
                    // Determine which triangle to remove
                    const uint32_t triangle_to_remove = detail::identify_triangle_to_remove(
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
    dedup_by_sort(triangles_to_remove);

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
    const bool ignore_orientation) {
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
    const bool ignore_orientation) {
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
    const std::vector<glm::vec<3, T>> &positions) {
    remove_duplicate_triangles<T>(triangles, std::span<const glm::vec<3, T>>(positions));
}
template <typename T>
void remove_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh) {
    remove_duplicate_triangles_consider_orientation(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
size_t remove_isolated_vertices(mesh::Simple_<n_dims, T> &mesh) {
    const bool has_uvs = mesh.has_uvs();
    const std::vector<uint32_t> isolated = find_isolated_vertices(mesh);
    DEBUG_ASSERT(std::is_sorted(isolated.begin(), isolated.end()));

    for (uint32_t i : isolated | std::views::reverse) {
        const uint32_t last_index = static_cast<uint32_t>(mesh.positions.size() - 1);

        if (i != last_index) {
            std::swap(mesh.positions[i], mesh.positions[last_index]);
            if (has_uvs) {
                std::swap(mesh.uvs[i], mesh.uvs[last_index]);
            }

            for (glm::uvec3 &triangle : mesh.triangles) {
                change_vertex_inplace(triangle, last_index, i);
            }
        }

        mesh.positions.pop_back();
        if (has_uvs) {
            mesh.uvs.pop_back();
        }
    }

    return isolated.size();
}

template <glm::length_t n_dims, typename T>
size_t remove_triangles_of_negligible_size(
    mesh::Simple_<n_dims, T> &mesh,
    const T threshold_percentage_of_average) {
    const size_t triangle_count = mesh.triangles.size();

    std::vector<T> areas;
    areas.reserve(triangle_count);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        const T area = geometry::compute_triangle_area(triangle, mesh.positions);
        areas.push_back(area);
    }

    const T total_area = std::reduce(areas.begin(), areas.end(), static_cast<T>(0));
    const T average_area = total_area / static_cast<T>(areas.size());

    const size_t erased_count = std::erase_if(mesh.triangles,
        [&](const glm::uvec3 &tri) {
            const size_t idx = &tri - &mesh.triangles.front();
            return areas[idx] < average_area * threshold_percentage_of_average;
        });
    return erased_count;
}

}
