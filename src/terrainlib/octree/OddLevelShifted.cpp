#include "octree/OddLevelShifted.h"
#include "octree/Id.h"

namespace octree {

namespace detail {
// When we shift odd levels, we need extra nodes at the max side to cover the same volume.
// However, we dont want to actually have more nodes since that wouldnt be an octree anymore.
// So we instead modify the bounds of the root node to be larger, so that we can just ignore the uncovered space
// since its outside the bounds anyways.
Bounds get_actual_bounds(const Bounds& original_bounds) {
    const glm::dvec3 new_min = original_bounds.min;
    const glm::dvec3 new_max = original_bounds.max + (original_bounds.size() / 4.0);
    return Bounds(new_min, new_max);
}
}

OddLevelShifted::OddLevelShifted(Bounds bounds) : _original_bounds(bounds), _space(detail::get_actual_bounds(bounds)) {}

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
        return Bounds(bounds.min - node_size / 2.0, bounds.max + node_size / 2.0);
    }
}
} // namespace detail

std::optional<Id> OddLevelShifted::find_node_at_level_containing_point(const glm::dvec3 &point, const uint32_t target_level) const {
    DEBUG_ASSERT(target_level <= Id::max_level());

    if (!this->_original_bounds.contains(point)) {
        return std::nullopt;
    }

    const Bounds bounds = detail::get_base_bounds_at_level(this->_space, target_level);
    const glm::dvec3 node_size = this->_space.get_node_size_at_level(target_level);
    const glm::dvec3 relative_position = (point - bounds.min) / node_size;
    const uint32_t resolution = Id::max_coord_on_level(target_level) + 1;
    const glm::uvec3 coords = relative_position * glm::dvec3(resolution);
    const Id id(target_level, coords);
    DEBUG_ASSERT(this->get_node_bounds(id).contains(point));
    return id;
}

std::vector<Id> OddLevelShifted::get_intersecting_nodes_on_next_level(const Id &id) const {
    const uint32_t level = id.level();

    std::vector<Id> result;
    auto r = id.child(0);
    if (!r.has_value()) {
        return result;
    }
    const Id base_child = r.value();
    const int32_t min_d = (level % 2 == 0) ? 0 : -1;
    const int32_t max_d = (level % 2 == 0) ? 2 : 0;
    const uint32_t count_d = max_d - min_d + 1;
    result.reserve(count_d * count_d * count_d);
    for (int32_t dx = min_d; dx <= max_d; dx++) {
        for (int32_t dy = min_d; dy <= max_d; dy++) {
            for (int32_t dz = min_d; dz <= max_d; dz++) {
                if (r = base_child.neighbour(glm::ivec3(dx, dy, dz)); !r) {
                    continue;
                }
                const Id child_id = r.value();
                DEBUG_ASSERT(radix::geometry::intersect(this->get_node_bounds(child_id), this->get_node_bounds(id)));
                result.push_back(child_id);
            }
        }
    }

    return result;
}
std::vector<Id> OddLevelShifted::get_intersecting_nodes_on_previous_level(const Id &id) const {
    // If there is no parent, return empty
    auto r = id.parent();
    if (!r.has_value()) {
        return {};
    }
    const Id parent_id = r.value();

    const uint32_t level = id.level();
    // For even levels, we can directly use the parent node
    if (level % 2 == 0) {
        const glm::ivec3 offset(
            (id.coords().x % 2 == 0) ? 0 : 1,
            (id.coords().y % 2 == 0) ? 0 : 1,
            (id.coords().z % 2 == 0) ? 0 : 1);
        if (auto r = parent_id.neighbour(offset); r.has_value()) {
            const Id neighbour_id = r.value();
            DEBUG_ASSERT(radix::geometry::intersect(this->get_node_bounds(neighbour_id), this->get_node_bounds(id)));
            return {neighbour_id};
        } else {
            return {};
        }
    }

    std::vector<Id> result;
    result.reserve(4);
    const glm::uvec3 coords = id.coords();
    const glm::ivec3 min_d(
        coords.x % 2 == 0 ? -1 : 0,
        coords.y % 2 == 0 ? -1 : 0,
        coords.z % 2 == 0 ? -1 : 0);
    for (int32_t dx = min_d.x; dx <= 0; dx++) {
        for (int32_t dy = min_d.y; dy <= 0; dy++) {
            for (int32_t dz = min_d.z; dz <= 0; dz++) {
                if (r = parent_id.neighbour(glm::ivec3(dx, dy, dz)); !r) {
                    continue;
                }
                const Id neighbour_id = r.value();
                DEBUG_ASSERT(radix::geometry::intersect(this->get_node_bounds(neighbour_id), this->get_node_bounds(id)));
                result.push_back(neighbour_id);
            }
        }
    }

    return result;
}

std::vector<Id> OddLevelShifted::get_intersecting_nodes_on_level(const Id &id, const uint32_t target_level) const {
    DEBUG_ASSERT(target_level <= Id::max_level());

    const uint32_t level = id.level();
    const glm::uvec3 coords = id.coords();

    // Same level -> trivial
    if (target_level == level) {
        return {id};
    }

    // Odd levels are shifted by -0.5, even levels are aligned
    const double source_offset = (level % 2 == 0) ? 0.0 : -0.5;
    const double target_offset = (target_level % 2 == 0) ? 0.0 : -0.5;

    // Scale factor between levels
    // ldexp(1.0, n) == 2^n
    const int32_t level_diff = static_cast<int32_t>(target_level) - static_cast<int32_t>(level);
    const double scale = std::ldexp(1.0, level_diff);

    // Compute continuous bounds of the source node in target grid space
    const glm::dvec3 min_f = (glm::dvec3(coords) + glm::dvec3(source_offset)) * scale - glm::dvec3(target_offset);
    const glm::dvec3 max_f = (glm::dvec3(coords) + glm::dvec3(1.0 + source_offset)) * scale - glm::dvec3(target_offset);

    // Convert continuous half-open interval [min_f, max_f) to integer grid indices
    glm::ivec3 min_c(
        static_cast<int32_t>(std::floor(min_f.x)),
        static_cast<int32_t>(std::floor(min_f.y)),
        static_cast<int32_t>(std::floor(min_f.z)));
    glm::ivec3 max_c(
        static_cast<int32_t>(std::ceil(max_f.x)) - 1,
        static_cast<int32_t>(std::ceil(max_f.y)) - 1,
        static_cast<int32_t>(std::ceil(max_f.z)) - 1);

    // Clamp to valid coordinate range on target level
    const int32_t max_coord = Id::max_coord_on_level(target_level);
    min_c = glm::max(min_c, glm::ivec3(0));
    max_c = glm::min(max_c, glm::ivec3(max_coord));

    std::vector<Id> result;

    // Preallocate exact number of candidates
    const glm::ivec3 extent = max_c - min_c + glm::ivec3(1);
    result.reserve(extent.x * extent.y * extent.z);

    // Enumerate all intersecting cells
    for (int32_t x = min_c.x; x <= max_c.x; x++) {
        for (int32_t y = min_c.y; y <= max_c.y; y++) {
            for (int32_t z = min_c.z; z <= max_c.z; z++) {
                result.push_back(Id(target_level, glm::uvec3(x, y, z)));
            }
        }
    }

    return result;
}

Bounds OddLevelShifted::get_node_bounds(const Id &id) const {
    const Bounds node_bounds = this->_space.get_node_bounds(id);
    if (id.level() % 2 == 0) {
        return node_bounds;
    } else {
        // Shift odd levels by half a node size 
        const glm::dvec3 shift = node_bounds.size() / 2.0;
        const Bounds shifted_bounds(node_bounds.min - shift, node_bounds.max - shift);
        const glm::uvec3 coords = id.coords();
        // Clip by original bounds
        if (glm::any(glm::equal(coords, glm::uvec3(0)))) {
            return radix::geometry::intersection(shifted_bounds, this->_original_bounds);
        }
        return shifted_bounds;
    }
}

const Bounds &OddLevelShifted::bounds() const {
    return this->_original_bounds;
}

} // namespace octree
