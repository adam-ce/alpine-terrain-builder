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

#include <glm/common.hpp>

namespace spatial_lookup {

template <typename T, glm::length_t n_dims, typename Component, typename Value>
concept SpatialLookup = requires(
    T t,
    const T ct,
    const glm::vec<n_dims, Component> &point,
    Value value,
    Component epsilon) {
    { t.clear() } -> std::same_as<void>;
    { t.insert(point, value) } -> std::same_as<bool>;

    {
        t.for_all_near(point, epsilon, [](const glm::vec<n_dims, Component> &, Value &, const Component) {})
    } -> std::same_as<bool>;
    {
        ct.for_all_near(point, epsilon, [](const glm::vec<n_dims, Component> &, const Value &, const Component) {})
    } -> std::same_as<bool>;
};

} // namespace spatial_lookup
