#include <optional>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "FixedVector.h"
#include "octree/Id.h"
#include "octree/IdRect.h"
#include "octree/OddLevelShifted.h"
#include "octree/Space.h"

namespace octree {

constexpr double RELATIVE_SHIFT = -0.5;

OddLevelShifted::OddLevelShifted(Bounds bounds) : _space(bounds) {}

OddLevelShifted OddLevelShifted::earth() {
    return OddLevelShifted(Space::earth().bounds());
}

namespace detail {
Bounds get_base_bounds_at_level(const Space& space, const uint32_t level) {
    const Bounds bounds = space.bounds();
    if (level % 2 == 0) {
        return bounds;
    } else {
        const glm::dvec3 node_size = space.get_node_size_at_level(level);
        const glm::dvec3 node_shift = node_size * RELATIVE_SHIFT;
        return Bounds(bounds.min + node_shift, bounds.max + node_shift);
    }
}
} // namespace detail

std::optional<Id> OddLevelShifted::find_node_at_level_containing_point(const glm::dvec3 &point, const uint32_t target_level) const {
    DEBUG_ASSERT(target_level <= Id::max_level());

    if (!this->contains(point)) {
        return std::nullopt;
    }

    const Bounds bounds = detail::get_base_bounds_at_level(this->_space, target_level);
    const glm::dvec3 node_size = this->_space.get_node_size_at_level(target_level);
    const glm::dvec3 relative_position = point / node_size - bounds.min / node_size;
    const glm::uvec3 coords = glm::min(glm::uvec3(relative_position), Id::max_coords_on_level(target_level));
    const Id id(target_level, coords);
    DEBUG_ASSERT(this->get_node_bounds(id).contains(point));
    return id;
}

IdRect OddLevelShifted::get_intersecting_nodes_on_level(const Id &id, const uint32_t target_level) const {
    DEBUG_ASSERT(target_level <= Id::max_level());

    if (target_level == id.level()) {
        return IdRect(id, id);
    }

    const Bounds source_bounds = this->get_node_bounds(id);
    const glm::dvec3 node_size = this->_space.get_node_size_at_level(target_level);
    const glm::dvec3 offset = node_size / 1024.0;

    const Id min_id = this->find_node_at_level_containing_point(source_bounds.min + offset, target_level).value();
    const Id max_id = this->find_node_at_level_containing_point(source_bounds.max - offset, target_level).value();
    return IdRect(min_id, max_id);
}

IdRect OddLevelShifted::get_intersecting_nodes_on_level(const Bounds &source_bounds, const uint32_t target_level) const {
    DEBUG_ASSERT(target_level <= Id::max_level());

    const glm::dvec3 node_size = this->_space.get_node_size_at_level(target_level);
    const glm::dvec3 offset = node_size / 1024.0;

    const auto min_id = this->find_node_at_level_containing_point(source_bounds.min + offset, target_level);
    const auto max_id = this->find_node_at_level_containing_point(source_bounds.max - offset, target_level);
    if (!min_id || !max_id) {
        return {};
    }
    return IdRect(*min_id, *max_id);
}

IdRect OddLevelShifted::find_intersecting_nodes_for_standard_id(const Id &id) const {
    if (id.level() % 2 == 0) {
        return IdRect(id, id);
    }
    const Bounds standard_bounds = this->_space.get_node_bounds(id);
    return this->get_intersecting_nodes_on_level(standard_bounds, id.level());
}

IdRect OddLevelShifted::find_intersecting_standard_nodes(const Id &id) const {
    if (id.level() % 2 == 0) {
        return IdRect(id, id);
    }

    // Odd levels are shifted by half a node size, so a shifted node at coords c
    // covers the same world space as standard nodes c-1 and c.
    const Id::Coords coords = id.coords();
    const Id::Coords min_coords = glm::max(coords, Id::Coords(1)) - Id::Coords(1);
    return IdRect(Id(id.level(), min_coords), id);
}

Bounds OddLevelShifted::get_node_bounds_with_children(const Id &id) const {
    if (!id.has_children()) {
        return this->get_node_bounds(id);
    }

    const Bounds first_bounds = this->get_node_bounds(id.child(0).value());
    const Bounds last_bounds = this->get_node_bounds(id.child(7).value());
    return Bounds(first_bounds.min, last_bounds.max);
}

Bounds OddLevelShifted::get_node_bounds(const Id &id) const {
    const Bounds node_bounds = this->_space.get_node_bounds(id);
    if (id.level() % 2 == 0) {
        return node_bounds;
    } else {
        // Shift odd levels by half a node size 
        const glm::dvec3 shift = node_bounds.size() / 2.0;
        Bounds shifted_bounds(node_bounds.min - shift, node_bounds.max - shift);

        // Clip or extend bounds on border
        const Id::Level level = id.level();
        const glm::uvec3 coords = id.coords();
        for (uint8_t k = 0; k < 3; k++) {
            // If on negative border -> clip against original bounds
            if (coords[k] == 0) {
                shifted_bounds.min[k] = this->bounds().min[k];
            }

            // If on positive border -> extend to original bounds
            if (coords[k] == Id::max_coord_on_level(level)) {
                shifted_bounds.max[k] = this->bounds().max[k];
            }
        }

        return shifted_bounds;
    }
}

const Bounds &OddLevelShifted::bounds() const {
    return this->_space.bounds();
}

bool OddLevelShifted::contains(const glm::dvec3 &point) const {
    return this->_space.contains(point);
}

} // namespace octree
