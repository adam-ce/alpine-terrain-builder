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

#include <optional>
#include <vector>

#include <radix/geometry.h>
#include <glm/glm.hpp>

#include "octree/Id.h"
#include "octree/Space.h"
#include "octree/IdRect.h"

namespace octree {
using Bounds = radix::geometry::Aabb3d;

// Odd-levels are shifted by 50% in the negative direction.
class OddLevelShifted {
public:
    explicit OddLevelShifted(Bounds bounds);
    static OddLevelShifted earth();

    std::optional<Id> find_node_at_level_containing_point(const glm::dvec3 &point, const uint32_t target_level) const;
    IdRect get_intersecting_nodes_on_level(const Id &id, const uint32_t level) const;
    IdRect get_intersecting_nodes_on_level(const Bounds &source_bounds, const uint32_t level) const;
    IdRect find_intersecting_nodes_for_standard_id(const Id &id) const;
    IdRect find_intersecting_standard_nodes(const Id &id) const;

    Bounds get_node_bounds_with_children(const Id &id) const;
    Bounds get_node_bounds(const Id &id) const;
    const Bounds &bounds() const;
    bool contains(const glm::dvec3& point) const;

private:
    const Space _space;

};

} // namespace octree
