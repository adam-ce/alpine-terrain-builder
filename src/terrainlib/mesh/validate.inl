#pragma once

#include "pch.h"
#include "mesh/utils.h"

#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <libassert/assert.hpp>

namespace mesh {

namespace {
template <glm::length_t n_dims, typename T>
void validate_sorted_normalized_mesh(const SimpleMesh_<n_dims, T> &mesh) {
    using Mesh = SimpleMesh_<n_dims, T>;
    using Triangle = typename Mesh::Triangle;
    using Position = typename Mesh::Position;
    using Uv = typename Mesh::Uv;
    static_assert(n_dims == 2 || n_dims == 3, "Mesh must be 2D or 3D");

    // Check correct count of uvs
    DEBUG_ASSERT(!mesh.has_uvs() || mesh.positions.size() == mesh.uvs.size());

    // Check uvs between 0 and 1
    for (const Uv &uv : mesh.uvs) {
        for (size_t k = 0; k < static_cast<size_t>(uv.length()); k++) {
            DEBUG_ASSERT(uv[k] >= 0);
            DEBUG_ASSERT(uv[k] <= 1);
        }
    }

    // Check for vertex indices in triangles outside valid range
    for (const Triangle &triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            const size_t vertex_index = triangle[k];
            DEBUG_ASSERT(vertex_index < mesh.vertex_count());
        }
    }

    // Check for degenerate triangles
    for (const Triangle &triangle : mesh.triangles) {
        DEBUG_ASSERT(triangle.x != triangle.y);
        DEBUG_ASSERT(triangle.y != triangle.z);
        DEBUG_ASSERT(triangle.x != triangle.z);
    }

    // Check for duplicated triangles
    DEBUG_ASSERT(mesh.triangles.end() == std::adjacent_find(mesh.triangles.begin(), mesh.triangles.end()));

    // Check for duplicated triangles with different orientations
    std::vector<Triangle> triangles_ignore_orientation(mesh.triangles);
    // Sort vertices in triangles
    for (Triangle &triangle : triangles_ignore_orientation) {
        std::sort(&triangle.x, &triangle.z + 1);
    }
    std::vector<Triangle> triangles_ignore_orientation_normalized(triangles_ignore_orientation);
    sort_and_normalize_triangles(triangles_ignore_orientation_normalized);
    DEBUG_ASSERT(triangles_ignore_orientation_normalized.end() == std::adjacent_find(triangles_ignore_orientation_normalized.begin(), triangles_ignore_orientation_normalized.end()));

    // Check for isolated vertices
    DEBUG_ASSERT(find_isolated_vertices(mesh).empty());

    // Check for duplicate vertices
    const double epsilon = 1e-9;
    auto almost_equal = [epsilon](const Position &a, const Position &b) {
        return glm::all(glm::lessThan(glm::abs(a - b), Position(epsilon)));
    };

    std::vector<Position> sorted_positions = mesh.positions;
    std::sort(sorted_positions.begin(), sorted_positions.end(), [&](const Position &a, const Position &b) {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        if constexpr (n_dims == 2) {
            return a.y < b.y;
        } else if constexpr (n_dims == 3) {
            if (a.y != b.y) {
                return a.y < b.y;
            }
            return a.z < b.z;
        }
    });

    for (size_t i = 1; i < sorted_positions.size(); i++) {
        DEBUG_ASSERT(!almost_equal(sorted_positions[i - 1], sorted_positions[i]));
    }
}
} // namespace

template <glm::length_t n_dims, typename T>
void validate(const SimpleMesh_<n_dims, T> &mesh) {
#ifndef NDEBUG
    SimpleMesh_<n_dims, T> sorted(mesh);
    sort_and_normalize_triangles(sorted);
    validate_sorted_normalized_mesh(sorted);
#else
    USE(mesh);
#endif
}

template <typename Point>
inline void validate(const CGAL::Surface_mesh<Point> &mesh) {
#ifndef NDEBUG
    DEBUG_ASSERT(mesh.is_valid()); 
    DEBUG_ASSERT(CGAL::is_triangle_mesh(mesh));
    DEBUG_ASSERT(CGAL::is_valid_polygon_mesh(mesh));
    DEBUG_ASSERT(!CGAL::Polygon_mesh_processing::does_self_intersect(mesh));
#else
    USE(mesh);
#endif
}
}
