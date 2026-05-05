#pragma once

#include <optional>
#include <vector>

#include <radix/geometry.h>
#include <glm/glm.hpp>

#include "octree/Id.h"
#include "octree/Space.h"

namespace octree {
using Bounds = radix::geometry::Aabb3d;

class OddLevelShifted {
public:
    explicit OddLevelShifted(Bounds bounds);
    static OddLevelShifted earth();

    std::optional<Id> find_node_at_level_containing_point(const glm::dvec3 &point, const uint32_t target_level) const;
    std::vector<Id> get_intersecting_nodes_on_next_level(const Id &id) const;
    std::vector<Id> get_intersecting_nodes_on_previous_level(const Id &id) const;
    std::vector<Id> get_intersecting_nodes_on_level(const Id &id, const uint32_t level) const;

    Bounds get_node_bounds(const Id &id) const;
    const Bounds& bounds() const;

private:
    const Bounds _original_bounds;
    const Space _space;

};

} // namespace octree
