#include "octree/Space.h"

namespace octree {

namespace detail {
template <typename T>
inline bool leq_eps(const T a, const T b, const T eps) {
    return a <= b + eps;
}

inline bool contains_inclusive(const Bounds &outer, const Bounds &inner, const double epsilon = 0) {
    return leq_eps(outer.min.x, inner.min.x, epsilon) &&
           leq_eps(inner.max.x, outer.max.x, epsilon) &&
           leq_eps(outer.min.y, inner.min.y, epsilon) &&
           leq_eps(inner.max.y, outer.max.y, epsilon) &&
           leq_eps(outer.min.z, inner.min.z, epsilon) &&
           leq_eps(inner.max.z, outer.max.z, epsilon);
};
} // namespace detail

Space::Space(Bounds bounds) : _bounds(bounds) {
    DEBUG_ASSERT(glm::all(glm::greaterThan(this->_bounds.size(), glm::dvec3(0))));
}

Space Space::earth() {
    constexpr float max_radius = 6384400; // from https://en.wikipedia.org/wiki/Summits_farthest_from_the_Earth%27s_center#:~:text=Dormant%20Volcano,6%2C267%20metres%20(20%2C561%20ft)
    constexpr float extends = max_radius * 1.1;
    return Space(Bounds(glm::dvec3(-extends), glm::dvec3(extends)));
}
std::optional<Id> Space::find_smallest_node_encompassing_bounds(
    const Bounds &target_bounds,
    const Id root) const {
    const glm::dvec3 target_size = target_bounds.size();

    if (target_size.x == 0 || target_size.y == 0 || target_size.z == 0) {
        throw std::invalid_argument("target bounds cannot be empty");
    }

    const Bounds root_bounds = get_node_bounds(root);
    if (!detail::contains_inclusive(root_bounds, target_bounds)) {
        return std::nullopt;
    }

    double epsilon = 1e-6 * glm::compMin(root_bounds.size());
    Id current = root;
    while (current.has_children()) {
        std::optional<Id> next;

        const auto children = current.children().value();

        for (const Id &child : children) {
            const Bounds child_bounds = get_node_bounds(child);
            if (detail::contains_inclusive(child_bounds, target_bounds, epsilon)) {
                next = child;
                break;
            }
        }

        if (!next.has_value()) {
            break;
        }

        current = next.value();
        epsilon /= 2;
    }

    return current;
}

std::optional<Id> Space::find_node_at_level_containing_point(const glm::dvec3& point, const uint32_t target_level, const Id root) const {
    DEBUG_ASSERT(target_level <= Id::max_level());

    Id current = root;
    const Bounds root_bounds = this->get_node_bounds(current);
    if (!root_bounds.contains_exclusive(point)) {
        return std::nullopt;
    }

    // TODO: this could be much more efficient
    while (current.level() < target_level) {
        const auto children = current.children().value();
        for (const auto& child : children) {
            const Bounds child_bounds = this->get_node_bounds(child);
            if (child_bounds.contains_exclusive(point)) {
                current = child;
                break;
            }
        }
    }

    return current;
}

glm::dvec3 Space::get_node_size_at_level(const uint32_t level) const {
    const uint32_t resolution = 1 << level;
    const glm::dvec3 bounds_size = this->_bounds.size();
    return bounds_size / glm::dvec3(resolution);
}


Bounds Space::get_node_bounds(const Id &id) const {
    const uint32_t level = id.level();
    const glm::uvec3 coords = id.coords();
    const glm::dvec3 node_size = this->get_node_size_at_level(level);

    // Calculate the minimum point of the node
    const glm::dvec3 min_bound = this->_bounds.min;
    const glm::dvec3 node_min = min_bound + glm::dvec3(coords) * node_size;

    // Calculate the maximum point of the node
    const glm::dvec3 node_max = node_min + node_size;

    return Bounds(node_min, node_max);
}

const Bounds& Space::bounds() const {
    return this->_bounds;
}

bool Space::contains(const glm::dvec3 &point) const {
    return this->_bounds.contains(point);
}

} // namespace octree
