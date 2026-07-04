#pragma once

#include <type_traits>

#include <glm/common.hpp>

#include "build_config.h"
#include "mesh/validate.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

namespace detail {

template <glm::length_t n_dims, typename T>
inline constexpr bool validate_supported_v = false;

template <>
inline constexpr bool validate_supported_v<2, float> = true;
template <>
inline constexpr bool validate_supported_v<2, double> = true;
template <>
inline constexpr bool validate_supported_v<3, float> = true;
template <>
inline constexpr bool validate_supported_v<3, double> = true;

template <glm::length_t n_dims, typename T>
void validate_impl(const mesh::View_<n_dims, T> &mesh, const ValidationFlags flags);

// These impls have been moved to the cpp file, since we want to keep this file cheap since it is
// included in lots of other files, and the validation code is unused in release anyways.
extern template void validate_impl<2, float>(const mesh::View_<2, float> &, ValidationFlags);
extern template void validate_impl<2, double>(const mesh::View_<2, double> &, ValidationFlags);
extern template void validate_impl<3, float>(const mesh::View_<3, float> &, ValidationFlags);
extern template void validate_impl<3, double>(const mesh::View_<3, double> &, ValidationFlags);

template <glm::length_t n_dims, typename T>
void validate_core(const mesh::View_<n_dims, T> &mesh, const ValidationFlags flags) {
    static_assert(validate_supported_v<n_dims, T>, "mesh::validate is only supported for (2,float), (2,double), (3,float), (3,double)");
    validate_impl<n_dims, T>(mesh, flags);
}

} // namespace detail

template <glm::length_t n_dims, typename T>
void validate(const mesh::View_<n_dims, T> &mesh, const ValidationFlags flags) {
    if constexpr (IS_DEBUG_BUILD) {
        detail::validate_core(mesh, flags);
    } else {
        USE(mesh);
        USE(flags);
    }
}
template <glm::length_t n_dims, typename T>
void validate(const mesh::Simple_<n_dims, T> &mesh, const ValidationFlags flags) {
    validate(mesh::View_<n_dims, T>(mesh), flags);
}

template <glm::length_t n_dims, typename T>
void validate_basic(const mesh::View_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic);
}
template <glm::length_t n_dims, typename T>
void validate_basic(const mesh::Simple_<n_dims, T> &mesh) {
    validate_basic(mesh::View_<n_dims, T>(mesh));
}

template <glm::length_t n_dims, typename T>
void validate_unconnected(const mesh::View_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic | ValidationFlags::Geometry);
}
template <glm::length_t n_dims, typename T>
void validate_unconnected(const mesh::Simple_<n_dims, T> &mesh) {
    validate_unconnected(mesh::View_<n_dims, T>(mesh));
}

template <glm::length_t n_dims, typename T>
void validate_connected(const mesh::View_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic | ValidationFlags::Geometry | ValidationFlags::SingleComponent);
}
template <glm::length_t n_dims, typename T>
void validate_connected(const mesh::Simple_<n_dims, T> &mesh) {
    validate_connected(mesh::View_<n_dims, T>(mesh));
}

template <glm::length_t n_dims, typename T>
void validate_manifold(const mesh::View_<n_dims, T> &mesh) {
    validate(mesh, ValidationFlags::Basic | ValidationFlags::Geometry | ValidationFlags::SingleComponent | ValidationFlags::Manifold);
}
template <glm::length_t n_dims, typename T>
void validate_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    validate_manifold(mesh::View_<n_dims, T>(mesh));
}

} // namespace mesh

