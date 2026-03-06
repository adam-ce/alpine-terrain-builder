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

#include "mesh/SimpleMesh.h"
#include "mesh/topology.h"
#include "mesh/geometry.h"
#include "vector_utils.h"
#include "HybridVector.h"

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
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh) {
    return find_duplicate_triangles_consider_orientation(mesh.triangles);
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
                const bool are_equal = compare_equality_triangles_ignore_orientation(
                    triangles[a], triangles[b]);
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

template <glm::length_t n_dims, typename T, typename Size = float>
size_t remove_isolated_vertices(SimpleMesh_<n_dims, T> &mesh) {
    const bool has_uvs = mesh.has_uvs();
    const std::vector<uint32_t> isolated = find_isolated_vertices(mesh);

    std::vector<uint32_t> index_offset;
    for (uint32_t i : isolated | std::views::reverse) {
        const uint32_t last_index = mesh.positions.size() - 1;
        std::swap(mesh.positions[i], mesh.positions[last_index]);
        mesh.positions.pop_back();
        if (has_uvs) {
            std::swap(mesh.uvs[i], mesh.uvs[last_index]);
        }
        mesh.uvs.pop_back();

        for (glm::uvec3 &triangle : mesh.triangles) {
            for (uint8_t k = 0; k < 3; k++) {
                if (triangle[k] == last_index) {
                    triangle[k] = i;
                }
            }
        }
    }

    return isolated.size();
}

template <glm::length_t n_dims, typename T, typename Size>
size_t remove_triangles_of_negligible_size(
    SimpleMesh_<n_dims, T> &mesh,
    const Size threshold_percentage_of_average) {
    const size_t triangle_count = mesh.triangles.size();

    std::vector<Size> areas;
    areas.reserve(triangle_count);

    for (const glm::uvec3 &tri : mesh.triangles) {
        const glm::tvec3<Size> p0 = glm::tvec3<Size>(mesh.positions[tri.x]);
        const glm::tvec3<Size> p1 = glm::tvec3<Size>(mesh.positions[tri.y]);
        const glm::tvec3<Size> p2 = glm::tvec3<Size>(mesh.positions[tri.z]);

        const Size twice_area = p0.x * (p1.y - p2.y) + p1.x * (p2.y - p0.y) + p2.x * (p0.y - p1.y);
        const Size area = static_cast<Size>(0.5) * std::abs(twice_area);
        areas.push_back(area);
    }

    const Size total_area = std::reduce(areas.begin(), areas.end(), static_cast<Size>(0));
    const Size average_area = total_area / static_cast<Size>(areas.size());

    const size_t erased_count = std::erase_if(mesh.triangles,
        [&](const glm::uvec3 &tri) {
            const size_t idx = &tri - &mesh.triangles.front();
            return areas[idx] < average_area * threshold_percentage_of_average;
        });
    return erased_count;
}
