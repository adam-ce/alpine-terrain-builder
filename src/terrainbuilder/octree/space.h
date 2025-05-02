#pragma once

#include <radix/geometry.h>

#include "id.h"

namespace octree {
using Bounds = radix::geometry::Aabb3d;

class Space {
public:
    const Bounds bounds;

    static constexpr Space earth() {
        const float max_radius = 6384400; // from https://en.wikipedia.org/wiki/Summits_farthest_from_the_Earth%27s_center#:~:text=Dormant%20Volcano,6%2C267%20metres%20(20%2C561%20ft)
        const float extends = max_radius * 1.1; // TODO: how much padding do we want here
        return Space(Bounds(
            {-extends, -extends, -extends}, {extends, extends, extends}));
    }

    std::optional<Id> find_smallest_node_encompassing_bounds(const Bounds &target_bounds, const Id root = Id::root()) const {
        // We don't want to recurse indefinitely if the bounds are empty.
        const glm::dvec3 target_size = target_bounds.size();
        if (target_size.x == 0 || target_size.y == 0 || target_size.z == 0) {
            throw std::invalid_argument("target bounds cannot be empty");
        }

        const std::array<glm::dvec3, 8> corners = radix::geometry::corners(target_bounds);

        const Bounds root_bounds = get_node_bounds(root);
        // Check if all points of the target bounds are inside the root bounds.
        bool all_corners_inside_root = true;
        for (const auto &corner : corners) {
            if (!root_bounds.contains_inclusive(corner)) {
                all_corners_inside_root = false;
                break;
            }
        }

        if (!all_corners_inside_root) {
            return std::nullopt; // Target bounds are outside the defined space.
        }

        Id current_smallest_encompassing_node = root;
        while (true) {
            const std::array<Id, 8> children = current_smallest_encompassing_node.children();
            std::optional<Id> next_smallest;

            for (const auto &child : children) {
                const Bounds child_bounds = get_node_bounds(child);
                bool all_corners_inside_child = true;
                for (const auto &corner : corners) {
                    if (!child_bounds.contains_inclusive(corner)) {
                        all_corners_inside_child = false;
                        break;
                    }
                }

                if (all_corners_inside_child) {
                    next_smallest = child;
                    break; // Found a child that fully contains the bounds, go deeper.
                }
            }

            if (next_smallest.has_value()) {
                current_smallest_encompassing_node = next_smallest.value();
            } else {
                break; // No child fully contains the bounds, so the current node is the smallest.
            }
        }

        return current_smallest_encompassing_node;
    }
    
    std::optional<Id> find_node_at_level_containing_point(const glm::dvec3& point, const uint32_t target_level, const Id root = Id::root()) const {
        Id current = root;
        const Bounds root_bounds = this->get_node_bounds(current);
        if (!root_bounds.contains_exclusive(point)) {
            return std::nullopt;
        }

        while (current.level() < target_level) {
            for (const auto& child : current.children()) {
                const Bounds child_bounds = this->get_node_bounds(child);
                if (child_bounds.contains_exclusive(point)) {
                    current = child;
                    break;
                }
            }
        }

        return current;
    }

    Bounds get_node_bounds(const Id &id) const {
        const auto coords = id.coords();
        const uint32_t level = id.level();
        const uint32_t resolution = 1 << level;

        // Calculate the size of a node at this level
        const glm::dvec3 bounds_size = bounds.size();
        const glm::dvec3 node_size = bounds_size / glm::dvec3(resolution);

        // Calculate the minimum point of the node
        const glm::dvec3 min_bound = bounds.min;
        const glm::dvec3 node_min = min_bound + glm::dvec3(coords) * node_size;

        // Calculate the maximum point of the node
        const glm::dvec3 node_max = node_min + node_size;

        Bounds node_bounds;
        node_bounds.min = node_min;
        node_bounds.max = node_max;
        return node_bounds;
    }

private:
};

} // namespace octree
