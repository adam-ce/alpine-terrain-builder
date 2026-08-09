#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/cleanup.h"
#include "mesh/topology/connected_components.h"
#include "mesh/convert.h"
#include "mesh/geometry.h"
#include "mesh/topology/manifold.h"
#include "mesh/validate.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {
namespace {
inline constexpr double EPSILON = 1e-12;

constexpr bool has_flag(const ValidationFlags set, const ValidationFlags flag) noexcept {
    return (set & flag) != ValidationFlags::None;
}

inline bool has_duplicate_faces(const std::span<const glm::uvec3> triangles, const bool ignore_orientation = true) {
    std::unordered_map<glm::uvec3, uint32_t> counts;
    counts.reserve(triangles.size());

    for (const glm::uvec3 &triangle : triangles) {
        const glm::uvec3 normalized = normalize_triangle(triangle, !ignore_orientation);
        counts[normalized] += 1;
    }

    for (const auto &[_triangle, count] : counts) {
        if (count > 1) {
            return true;
        }
    }

    return false;
}

template <glm::length_t n_dims, typename T>
void validate_impl_basic(const mesh::View_<n_dims, T> &mesh) {
    using Mesh = mesh::View_<n_dims, T>;
    using Triangle = typename Mesh::Triangle;
    using Uv = typename Mesh::Uv;

    static_assert(n_dims == 2 || n_dims == 3, "Mesh must be 2D or 3D");

    if (mesh.has_uvs()) {
        DEBUG_ASSERT(mesh.positions.size() == mesh.uvs.size());
    }

    for (const Uv &uv : mesh.uvs) {
        for (uint8_t k = 0; k < 2; k++) {
            DEBUG_ASSERT(uv[k] >= static_cast<typename Uv::value_type>(0));
            DEBUG_ASSERT(uv[k] <= static_cast<typename Uv::value_type>(1));
        }
    }

    const size_t vertex_count = mesh.vertex_count();
    for (const Triangle &triangle : mesh.triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            const size_t vertex_index = triangle[k];
            DEBUG_ASSERT(vertex_index < vertex_count);
        }
    }

    for (const Triangle &triangle : mesh.triangles) {
        DEBUG_ASSERT(!is_degenerate(triangle));
    }
}

template <glm::length_t n_dims, typename T>
void validate_impl_topology(const mesh::View_<n_dims, T> &mesh, const ValidationFlags flags) {
    if (has_flag(flags, ValidationFlags::SingleComponent)) {
        DEBUG_ASSERT(is_single_component(mesh));
    }

    if (has_flag(flags, ValidationFlags::Manifold)) {
        DEBUG_ASSERT(is_manifold(mesh));
    }
}

template <glm::length_t n_dims, typename T>
void validate_impl_geometry(const mesh::View_<n_dims, T> &mesh) {
    static_assert(n_dims >= 2, "Geometry checks require n_dims >= 2");

    DEBUG_ASSERT(find_isolated_vertices(mesh).empty());

    /*
    While this check makes sense on its own
    This produces false positives when validating the geometry for uv unwrapping without uvs
    for (const glm::uvec3 &triangle : mesh.triangles) {
        const bool is_empty = is_empty_triangle(triangle, mesh.positions);
        if (mesh.has_uvs()) {
            DEBUG_ASSERT(!is_empty_triangle(triangle, mesh.uvs));
        } else {
            DEBUG_ASSERT(is_empty);
        }
    }
    */
}
}

namespace detail {

template <glm::length_t n_dims, typename T>
void validate_impl(const mesh::View_<n_dims, T> &mesh, const ValidationFlags flags) {
    if (has_flag(flags, ValidationFlags::Basic)) {
        validate_impl_basic(mesh);
    }

    validate_impl_topology(mesh, flags);

    if (has_flag(flags, ValidationFlags::Geometry)) {
        validate_impl_geometry(mesh);
    }
}

template void validate_impl<2, float>(const mesh::View_<2, float> &, ValidationFlags);
template void validate_impl<2, double>(const mesh::View_<2, double> &, ValidationFlags);
template void validate_impl<3, float>(const mesh::View_<3, float> &, ValidationFlags);
template void validate_impl<3, double>(const mesh::View_<3, double> &, ValidationFlags);

}
}
