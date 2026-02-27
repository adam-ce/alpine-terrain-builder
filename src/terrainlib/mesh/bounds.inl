#include <array>
#include <functional>
#include <limits>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"

namespace detail {
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
    return detail::calculate_bounds_range(std::array{std::cref(mesh)});
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes) {
    return detail::calculate_bounds_range(meshes);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes) {
    return detail::calculate_bounds_range(meshes);
}
