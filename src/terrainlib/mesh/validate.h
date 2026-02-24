#pragma once

#include "mesh/SimpleMesh.h"

namespace mesh {

enum class ValidationFlags : uint32_t {
    None = 0,
    Basic = 1u << 0,
    Geometry = 1u << 1,
    Manifold = 1u << 2,
    SingleComponent = 1u << 3,
    All = Basic | Geometry | Manifold | SingleComponent
};
constexpr ValidationFlags operator|(ValidationFlags a, ValidationFlags b) noexcept {
    return static_cast<ValidationFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr ValidationFlags operator&(ValidationFlags a, ValidationFlags b) noexcept {
    return static_cast<ValidationFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
constexpr ValidationFlags &operator|=(ValidationFlags &a, ValidationFlags b) noexcept {
    a = a | b;
    return a;
}

template <glm::length_t n_dims, typename T>
void validate(const SimpleMesh_<n_dims, T> &mesh, ValidationFlags flags = ValidationFlags::All);

template <glm::length_t n_dims, typename T>
void validate_connected_manifold(const SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
void validate_unconnected_nonmanifold(const SimpleMesh_<n_dims, T> &mesh);

} // namespace mesh

#include "mesh/validate.inl"
