#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "containers/SegmentedBuffer.h"
#include "enumerate.h"
#include "mesh/SimpleMesh.h"
#include "type_utils.h"
#include "range_utils.h"

namespace mesh {

namespace detail {
template <typename Mapping>
std::vector<uint32_t> calculate_triangle_counts(const uint32_t triangle_count, const uint32_t group_count, Mapping &&triangle_to_group) {
    std::vector<uint32_t> triangle_counts(group_count, 0);
    for (const uint32_t triangle_index : range(triangle_count)) {
        const uint32_t group_index = triangle_to_group(triangle_index);
        DEBUG_ASSERT(group_index < group_count);
        triangle_counts[group_index]++;
    }
    return triangle_counts;
}

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle_impl(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group) {
    const uint32_t vertex_count = mesh.vertex_count();
    const uint32_t triangle_count = mesh.face_count();

    // Allocate group meshes
    std::vector<mesh::Simple_<n_dims, T>> groups;
    groups.resize(group_count);

    const std::vector<uint32_t> triangle_counts = detail::calculate_triangle_counts(triangle_count, group_count, triangle_to_group);
    for (const auto [group_index, triangle_count] : enumerate(triangle_counts)) {
        mesh::Simple_<n_dims, T> &group = groups[group_index];
        group.triangles.reserve(triangle_count);
    };

    // Split triangles and remap/clone vertices
    const uint32_t invalid_vertex = std::numeric_limits<uint32_t>::max();
    Vector2D<uint32_t> vertex_remap(group_count, vertex_count, invalid_vertex);

    for (const auto [triangle_index, triangle] : enumerate(mesh.triangles)) {
        const uint32_t group_index = triangle_to_group(triangle_index);
        mesh::Simple_<n_dims, T> &group_mesh = groups[group_index];

        glm::uvec3 new_triangle;
        for (uint8_t k = 0; k < 3; k++) {
            const uint32_t vertex_index = triangle[k];
            uint32_t &new_vertex_index = vertex_remap(group_index, vertex_index);

            if (new_vertex_index == invalid_vertex) {
                new_vertex_index = group_mesh.positions.size();
                group_mesh.positions.push_back(mesh.positions[vertex_index]);
                if (mesh.has_uvs()) {
                    group_mesh.uvs.push_back(mesh.uvs[vertex_index]);
                }
            }

            new_triangle[k] = new_vertex_index;
        }

        group_mesh.triangles.push_back(new_triangle);
    }

    if (mesh.has_texture()) {
        for (auto &group_mesh : groups) {
            group_mesh.texture = mesh.texture.value();
        }
    }

    return SplitByTriangleResult<n_dims, T>{groups, vertex_remap};
}
}
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group) {
    if constexpr (std::is_invocable_v<Mapping, uint32_t>) {
        return detail::split_by_triangle_impl(mesh, group_count, std::forward<Mapping>(triangle_to_group));
    } else if constexpr (std::ranges::random_access_range<Mapping> &&
                         std::ranges::sized_range<Mapping>) {
        return detail::split_by_triangle_impl(mesh, group_count, [&](const uint32_t triangle_index) {
            DEBUG_ASSERT(triangle_index < std::ranges::size(triangle_to_group));
            return triangle_to_group[triangle_index];
        });
    } else {
        static_assert(always_false_v<Mapping>, "triangle_to_group must be a callable or a range/container.");
    }
}
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle(const mesh::Simple_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group) {
    return split_by_triangle(mesh::View_<n_dims, T>(mesh), group_count, std::forward<Mapping>(triangle_to_group));
}

namespace detail {
template <typename Mapping>
std::vector<uint32_t> calculate_vertex_counts(const uint32_t vertex_count, const uint32_t group_count, Mapping &&vertex_to_group) {
    std::vector<uint32_t> vertex_counts(group_count, 0);
    for (const uint32_t vertex_index : range(vertex_count)) {
        const uint32_t group_index = vertex_to_group(vertex_index);
        DEBUG_ASSERT(group_index < group_count);
        vertex_counts[group_index]++;
    }
    return vertex_counts;
}

template <typename Mapping>
std::vector<uint32_t> calculate_triangle_counts(const std::span<const glm::uvec3> triangles, const uint32_t group_count, Mapping &&vertex_to_group, const bool allow_mixed_triangles) {
    std::vector<uint32_t> triangle_counts(group_count, 0);
    for (const glm::uvec3 &triangle : triangles) {
        const uint32_t g0 = vertex_to_group(triangle[0]);
        const uint32_t g1 = vertex_to_group(triangle[1]);
        const uint32_t g2 = vertex_to_group(triangle[2]);

        DEBUG_ASSERT(g0 < group_count);
        DEBUG_ASSERT(g1 < group_count);
        DEBUG_ASSERT(g2 < group_count);

        if (!allow_mixed_triangles) {
            DEBUG_ASSERT(g0 == g1);
            DEBUG_ASSERT(g0 == g2);
        }

        triangle_counts[g0]++;
    }
    return triangle_counts;
}

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex_impl(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group, const bool drop_mixed_triangles) {
    const uint32_t vertex_count = mesh.vertex_count();

    // Allocate group meshes
    std::vector<mesh::Simple_<n_dims, T>> groups;
    groups.resize(group_count);

    const std::vector<uint32_t> vertex_counts = detail::calculate_vertex_counts(vertex_count, group_count, vertex_to_group);
    for (const auto [group_index, vertex_count] : enumerate(vertex_counts)) {
        mesh::Simple_<n_dims, T> &group = groups[group_index];
        group.positions.reserve(vertex_count);
        if (mesh.has_uvs()) {
            group.uvs.reserve(vertex_count);
        }
    }
    const std::vector<uint32_t> triangle_counts = detail::calculate_triangle_counts(mesh.triangles, group_count, vertex_to_group, drop_mixed_triangles);
    for (const auto [group_index, triangle_count] : enumerate(triangle_counts)) {
        mesh::Simple_<n_dims, T> &group = groups[group_index];
        group.triangles.reserve(triangle_count);
    };

    // Split vertices and create remap table
    const uint32_t invalid_vertex = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> vertex_remap(vertex_count, invalid_vertex);
    for (uint32_t vertex = 0; vertex < vertex_count; vertex++) {
        const uint32_t group_index = vertex_to_group(vertex);
        mesh::Simple_<n_dims, T> &group = groups[group_index];
        vertex_remap[vertex] = group.positions.size();
        group.positions.push_back(mesh.positions[vertex]);
        if (mesh.has_uvs()) {
            group.uvs.push_back(mesh.uvs[vertex]);
        }
    }

    // Split and remap triangles
    for (const glm::uvec3 &triangle : mesh.triangles) {
        const uint32_t group_index = vertex_to_group(triangle[0]);
        if (drop_mixed_triangles) {
            const uint32_t g0 = group_index;
            const uint32_t g1 = vertex_to_group(triangle[1]);
            const uint32_t g2 = vertex_to_group(triangle[2]);

            if (!(g0 == g1 && g0 == g2)) {
                continue;
            }
        }

        glm::uvec3 new_triangle;
        for (uint8_t k = 0; k < 3; k++) {
            new_triangle[k] = vertex_remap[triangle[k]];
        }

        mesh::Simple_<n_dims, T> &group = groups[group_index];
        group.triangles.push_back(new_triangle);
    }

    // Copy texture
    if (mesh.has_texture()) {
        for (mesh::Simple_<n_dims, T> &group : groups) {
            group.texture = mesh.texture.value();
        }
    }

    return SplitByVertexResult<n_dims, T>{groups, vertex_remap};
}
}

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group, const bool drop_mixed_triangles) {
    if constexpr (std::is_invocable_v<Mapping, uint32_t>) {
        return detail::split_by_vertex_impl(mesh, group_count, std::forward<Mapping>(vertex_to_group), drop_mixed_triangles);
    } else if constexpr (std::ranges::random_access_range<Mapping> &&
                         std::ranges::sized_range<Mapping>) {
        return detail::split_by_vertex_impl(mesh, group_count, [&](const uint32_t i) {
            DEBUG_ASSERT(i < std::ranges::size(vertex_to_group));
            return vertex_to_group[i]; }, drop_mixed_triangles);
    } else {
        static_assert(always_false_v<Mapping>, "vertex_to_group must be a callable or a range/container.");
    }
}
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex(const mesh::Simple_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group, const bool drop_mixed_triangles) {
    return split_by_vertex(mesh::View_<n_dims, T>(mesh), group_count, std::forward<Mapping>(vertex_to_group), drop_mixed_triangles);
}
template <glm::length_t n_dims, typename T>
SplitByVertexResult<n_dims, T> split_by_vertex(
    const mesh::View_<n_dims, T> &mesh,
    const uint32_t group_count,
    const std::span<const uint32_t> vertex_to_group_map,
    const bool drop_mixed_triangles = true) {
    return split_by_vertex(mesh, group_count, [&](const uint32_t vertex_index) {
        DEBUG_ASSERT(vertex_index < vertex_to_group_map.size());
        return vertex_to_group_map[vertex_index]; }, drop_mixed_triangles);
}

} // namespace mesh
