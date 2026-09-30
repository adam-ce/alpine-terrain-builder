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

#include <concepts>
#include <optional>
#include <vector>

#include <glm/common.hpp>
#include <radix/geometry.h>

namespace spatial_lookup {

template <
    typename T,
    glm::length_t n_dims,
    typename Component,
    typename Value>
concept CellBasedStorage = requires(
    T storage,
    const T const_storage,
    typename T::CellIndex index,
    Value value,
    const glm::vec<n_dims, Component> point,
    const glm::vec<n_dims, int32_t> offset) {
    typename T::CellIndex;

    { const_storage.point_to_cell_index(point) } -> std::same_as<typename T::CellIndex>;
    { const_storage.offset_cell_index(index, offset) } -> std::same_as<typename T::CellIndex>;
    { const_storage.cell_bounds(index) } -> std::same_as<radix::geometry::Aabb<n_dims, Component>>;

    { storage.insert(point, value) } -> std::same_as<bool>;

    {
        storage.for_all_in_cell(index, [](const glm::vec<n_dims, Component>&, Value&) {})
    } -> std::same_as<bool>;
    {
        const_storage.for_all_in_cell(index, [](const glm::vec<n_dims, Component>&, const Value&) {})
    } -> std::same_as<bool>;

    {
        storage.for_all_points([](const glm::vec<n_dims, Component> &, Value &) {})
    } -> std::same_as<bool>;

    {
        const_storage.for_all_points([](const glm::vec<n_dims, Component> &, const Value &) {})
    } -> std::same_as<bool>;
};

} // namespace spatial_lookup
