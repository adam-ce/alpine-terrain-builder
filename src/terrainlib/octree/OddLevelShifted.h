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

    Bounds get_node_bounds_with_children(const Id &id) const;
    Bounds get_node_bounds(const Id &id) const;
    const Bounds &bounds() const;
    bool contains(const glm::dvec3& point) const;

private:
    const Space _space;

};

} // namespace octree
