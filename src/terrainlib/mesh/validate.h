/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

enum class ValidationFlags : uint32_t {
    None = 0,
    Basic = 1u << 0,
    Geometry = 1u << 1,
    Manifold = 1u << 2,
    SingleComponent = 1u << 3,
    All = Basic | Geometry | Manifold | SingleComponent
};
constexpr ValidationFlags operator|(const ValidationFlags a, const ValidationFlags b) noexcept {
    return static_cast<ValidationFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr ValidationFlags operator&(const ValidationFlags a, const ValidationFlags b) noexcept {
    return static_cast<ValidationFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
constexpr ValidationFlags &operator|=(ValidationFlags &a, const ValidationFlags b) noexcept {
    a = a | b;
    return a;
}

template <glm::length_t n_dims, typename T>
void validate(const mesh::Simple_<n_dims, T> &mesh, const ValidationFlags flags = ValidationFlags::Basic | ValidationFlags::Geometry | ValidationFlags::Manifold);
template <glm::length_t n_dims, typename T>
void validate(const mesh::View_<n_dims, T> &mesh, const ValidationFlags flags = ValidationFlags::Basic | ValidationFlags::Geometry | ValidationFlags::Manifold);

template <glm::length_t n_dims, typename T>
void validate_basic(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void validate_basic(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
void validate_unconnected(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void validate_unconnected(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
void validate_connected(const mesh::View_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void validate_connected(const mesh::Simple_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
void validate_manifold(const mesh::View_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void validate_manifold(const mesh::Simple_<n_dims, T> &mesh);

} // namespace mesh

#include "mesh/validate.inl"
