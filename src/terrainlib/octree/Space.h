#pragma once

#include <optional>

#include <radix/geometry.h>
#include <glm/glm.hpp>

#include "octree/Id.h"

namespace octree {
using Bounds = radix::geometry::Aabb3d;

// TODO: add srs to this?
class Space {
public:
    explicit Space(Bounds bounds);
    static Space earth();

    std::optional<Id> find_smallest_node_encompassing_bounds(const Bounds &target_bounds, const Id root = Id::root()) const ;
    std::optional<Id> find_node_at_level_containing_point(const glm::dvec3& point, const uint32_t target_level, const Id root = Id::root()) const;

    glm::dvec3 get_node_size_at_level(const uint32_t level) const;
    Bounds get_node_bounds(const Id &id) const;
    const Bounds& bounds() const;

private:
    const Bounds _bounds;

};

} // namespace octree
