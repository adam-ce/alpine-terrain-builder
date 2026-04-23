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

namespace mesh {

namespace detail {
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> empty_bounds() {
    radix::geometry::Aabb<n_dims, T> bounds;
    constexpr T inf = std::numeric_limits<T>::infinity();
    bounds.min = glm::vec<n_dims, T>(+inf);
    bounds.max = glm::vec<n_dims, T>(-inf);
    return bounds;
}

template <typename MeshRange>
auto calculate_bounds_mesh_range(const MeshRange &meshes) {
    using Mesh = std::unwrap_reference_t<std::ranges::range_value_t<MeshRange>>;
    using Vec = std::remove_cvref_t<decltype(std::declval<Mesh>().positions[0])>;
    using T = typename Vec::value_type;
    constexpr glm::length_t n_dims = Vec::length();

    radix::geometry::Aabb<n_dims, T> bounds = detail::empty_bounds<n_dims, T>();

    for (const auto &mesh_ref : meshes) {
        const Mesh &mesh = mesh_ref;
        extend_bounds<n_dims, T>(bounds, mesh.positions);
    }

    return bounds;
}
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh) {
    return calculate_bounds(mesh.positions);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes) {
    return detail::calculate_bounds_mesh_range(meshes);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes) {
    return detail::calculate_bounds_mesh_range(meshes);
}

template <glm::length_t n_dims, typename T>
void extend_bounds(radix::geometry::Aabb<n_dims, T> &bounds, const std::span<const glm::vec<n_dims, T>> positions) {
    for (const auto &position : positions) {
        bounds.expand_by(position);
    }
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const glm::vec<n_dims, T>> positions) {
    radix::geometry::Aabb<n_dims, T> bounds = detail::empty_bounds<n_dims, T>();
    extend_bounds(bounds, positions);
    return bounds;
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<glm::vec<n_dims, T>> positions) {
    return calculate_bounds(std::span<const glm::vec<n_dims, T>>(positions));
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::vector<glm::vec<n_dims, T>>& positions) {
    return calculate_bounds(std::span<const glm::vec<n_dims, T>>(positions));
}

}
