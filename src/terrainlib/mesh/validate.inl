#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/connected_components.h"
#include "mesh/convert.h"
#include "mesh/geometry.h"
#include "mesh/manifold.h"
#include "mesh/cleanup.h"

namespace mesh {

namespace detail {

inline constexpr double EPSILON = 1e-12;

constexpr bool has_flag(ValidationFlags set, ValidationFlags flag) noexcept {
    return (set & flag) != ValidationFlags::None;
}

inline bool has_duplicate_faces(std::span<const glm::uvec3> triangles, bool ignore_orientation = true) {
    std::unordered_map<glm::uvec3, uint32_t> counts;
    counts.reserve(triangles.size());

    for (const glm::uvec3 &tri : triangles) {
        const glm::uvec3 normalized = normalize_triangle(tri, !ignore_orientation);
        counts[normalized] += 1u;
    }

    for (const auto &[_tri, count] : counts) {
        if (count > 1u) {
            return true;
        }
    }

    return false;
}

template <glm::length_t n_dims, typename T>
T doubled_area_squared(const glm::vec<n_dims, T> &a,
                       const glm::vec<n_dims, T> &b,
                       const glm::vec<n_dims, T> &c) {
    static_assert(n_dims == 2 || n_dims == 3);

    const glm::vec<n_dims, T> ab = b - a;
    const glm::vec<n_dims, T> ac = c - a;

    if constexpr (n_dims == 2) {
        const T cross = ab.x * ac.y - ab.y * ac.x;
        return cross * cross;
    } else {
        const glm::vec<3, T> cross = glm::cross(ab, ac);
        return glm::dot(cross, cross);
    }
}

template <glm::length_t n_dims, typename T>
void validate_basic(const SimpleMesh_<n_dims, T> &mesh) {
    using Mesh = SimpleMesh_<n_dims, T>;
    using Triangle = typename Mesh::Triangle;
    using Uv = typename Mesh::Uv;

    static_assert(n_dims == 2 || n_dims == 3, "Mesh must be 2D or 3D");

    if (mesh.has_uvs()) {
        DEBUG_ASSERT(mesh.positions.size() == mesh.uvs.size());
    }

    for (const Uv &uv : mesh.uvs) {
        for (glm::length_t k = 0; k < uv.length(); k++) {
            DEBUG_ASSERT(uv[k] >= static_cast<typename Uv::value_type>(0));
            DEBUG_ASSERT(uv[k] <= static_cast<typename Uv::value_type>(1));
        }
    }

    const size_t vertex_count = mesh.vertex_count();
    for (const Triangle &triangle : mesh.triangles) {
        for (glm::length_t k = 0; k < triangle.length(); k++) {
            const size_t vertex_index = static_cast<size_t>(triangle[k]);
            DEBUG_ASSERT(vertex_index < vertex_count);
        }
    }

    for (const Triangle &triangle : mesh.triangles) {
        DEBUG_ASSERT(!is_degenerate(triangle));
    }
}

template <glm::length_t n_dims, typename T>
void validate_topology(const SimpleMesh_<n_dims, T> &mesh, ValidationFlags flags) {
    if (has_flag(flags, ValidationFlags::SingleComponent)) {
        DEBUG_ASSERT(is_single_component(mesh));
    }

    if (has_flag(flags, ValidationFlags::Manifold)) {
        DEBUG_ASSERT(is_manifold(mesh));

        DEBUG_ASSERT(!has_duplicate_faces(std::span<const glm::uvec3>(mesh.triangles), true));
    }
}

template <glm::length_t n_dims, typename T>
void validate_geometry(const SimpleMesh_<n_dims, T> &mesh) {
    static_assert(n_dims >= 2, "Geometry checks require n_dims >= 2");

    DEBUG_ASSERT(find_isolated_vertices(mesh).empty());

    DEBUG_ASSERT(!has_duplicate_faces(std::span<const glm::uvec3>(mesh.triangles), false));

    const T double_epsilon_sq = static_cast<T>(4) * EPSILON * EPSILON;
    for (const auto &tri : mesh.triangles) {
        const auto &a = mesh.positions[static_cast<size_t>(tri[0])];
        const auto &b = mesh.positions[static_cast<size_t>(tri[1])];
        const auto &c = mesh.positions[static_cast<size_t>(tri[2])];

        const T double_area_sq = doubled_area_squared<n_dims, T>(a, b, c);
        DEBUG_ASSERT(double_area_sq > double_epsilon_sq);
    }
}

} // namespace detail

template <glm::length_t n_dims, typename T>
void validate(const SimpleMesh_<n_dims, T> &mesh, ValidationFlags flags) {
#ifndef NDEBUG
    if (detail::has_flag(flags, ValidationFlags::Basic)) {
        detail::validate_basic(mesh);
    }

    detail::validate_topology(mesh, flags);

    if (detail::has_flag(flags, ValidationFlags::Geometry)) {
        detail::validate_geometry(mesh);
    }
#endif

    USE(mesh);
    USE(flags);
}

template <glm::length_t n_dims, typename T>
void validate_basic(const SimpleMesh_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic);
}

template <glm::length_t n_dims, typename T>
void validate_unconnected(const SimpleMesh_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic | ValidationFlags::Geometry);
}

template <glm::length_t n_dims, typename T>
void validate_connected(const SimpleMesh_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic | ValidationFlags::Geometry | ValidationFlags::SingleComponent);
}

template <glm::length_t n_dims, typename T>
void validate_manifold(const SimpleMesh_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic | ValidationFlags::Geometry | ValidationFlags::SingleComponent | ValidationFlags::Manifold);
}

} // namespace mesh
