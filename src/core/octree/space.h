#pragma once

#include <radix/geometry.h>

#include "id.h"
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <limits>

namespace octree {
using Bounds = radix::geometry::Aabb3d;

struct OctreeRenderIntent {
    std::vector<float> instances_active;
    std::vector<glm::mat4> instances_model_mats;
    size_t instance_count;

    std::optional<double> min_scene_distance;
    std::optional<double> max_scene_distance;

    std::optional<Id> closest_node;
};

class Space {
public:
    const Bounds bounds;

    static constexpr Space earth() {
        const double max_radius = 6384400; // from https://en.wikipedia.org/wiki/Summits_farthest_from_the_Earth%27s_center#:~:text=Dormant%20Volcano,6%2C267%20metres%20(20%2C561%20ft)
        const double extends = max_radius * 1.1; // TODO: how much padding do we want here
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
        while (current_smallest_encompassing_node.has_children()) {
            const std::array<Id, 8> children = current_smallest_encompassing_node.children().value();
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
        assert(target_level <= Id::max_level());

        Id current = root;
        const Bounds root_bounds = this->get_node_bounds(current);
        if (!root_bounds.contains_exclusive(point)) {
            return std::nullopt;
        }

        while (current.level() < target_level && current.has_children()) {
            for (const auto& child : current.children().value()) {
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

    OctreeRenderIntent generate_octree_render_intent(const Id root, glm::dvec3 cam_pos, bool draw_neighbours_only) {
        // Defines under which ratio between dist(cam, node_bbox_centre) / node_bbox_size the node is split
        // I.e.: dist = 2000 and node_bbox_size = 4000 -> ratio = 0.5 -> node would get refined
        double refining_ratio = 1.0f;

        std::vector<Id> refining_ids;
        refining_ids.push_back(root);

        std::vector<float> instance_active;
        std::vector<glm::mat4> instance_models;

        double closest_distance = std::numeric_limits<double>::infinity();
        double farthest_distance = 0;
        std::optional<size_t> closest_index;

        std::optional<Id> closest;
        std::optional<Id> farthest;


        while (!refining_ids.empty()) {
            Id current = refining_ids.back();
            refining_ids.pop_back();

            Bounds current_bounds = get_node_bounds(current);
            
            double current_distance = glm::distance(current_bounds.centre(), cam_pos);
            double current_ratio = current_distance / glm::length(current_bounds.size());

            //LOG_DEBUG("DIST: {}, RATIO: {}", distance, current_ratio);

            if (current_ratio <= refining_ratio && current.has_children()) {
                // Split node into 8 children, and add them to the refining list
                for (Id child : current.children().value()) {
                    refining_ids.push_back(child);
                }
            } else {
                // Don't split. Check whether to render this node
                bool is_current_closest = false;
                bool is_current_farthest = false;

                if (current_distance < closest_distance) {
                    closest_distance = current_distance;
                    is_current_closest = true;
                    closest = current;
                }
                if (current_distance > farthest_distance) {
                    farthest_distance = current_distance;
                    is_current_farthest = true;
                    farthest = current;
                }

                if (!draw_neighbours_only) {
                    glm::dmat4 model_scale = glm::scale(glm::dmat4(1.0f), current_bounds.size());
                    glm::dmat4 model_translate = glm::translate(glm::dmat4(1.0f), current_bounds.centre());
                    glm::mat4 model = model_translate * model_scale;

                    instance_active.push_back(0.0f);
                    instance_models.push_back(glm::mat4(model));

                    if (is_current_closest) {
                        // The current node, is the closest to this point. Mark it as active and mark the previous closest (if any) as inactive
                        if (closest_index.has_value()) {
                            instance_active[closest_index.value()] = 0.0f;
                        }
                        closest_index = instance_active.size() - 1;
                        instance_active[closest_index.value()] = 1.0f;
                    }
                } else {
                    if (is_current_closest) {
                        // The current node, is the closest to this point. Delete all nodes in the list

                        instance_active.clear();
                        instance_models.clear();

                        farthest_distance = current_distance;
                        farthest = current;

                        // Add the current active node to the render list
                        glm::dmat4 model_scale = glm::scale(glm::dmat4(1.0f), current_bounds.size());
                        glm::dmat4 model_translate = glm::translate(glm::dmat4(1.0f), current_bounds.centre());
                        glm::mat4 model = model_translate * model_scale;

                        instance_active.push_back(1.0f);
                        instance_models.push_back(glm::mat4(model));

                        for (auto neighbour : current.neighbours()) {
                            Bounds neighbour_bounds = get_node_bounds(neighbour);

                            double neighbour_distance = glm::distance(neighbour_bounds.centre(), cam_pos);

                            glm::dmat4 neighbour_model_scale = glm::scale(glm::dmat4(1.0f), neighbour_bounds.size());
                            glm::dmat4 neighbour_model_translate = glm::translate(glm::dmat4(1.0f), neighbour_bounds.centre());
                            glm::mat4 neighbour_model = neighbour_model_translate * neighbour_model_scale;

                            instance_active.push_back(0.0f);
                            instance_models.push_back(glm::mat4(neighbour_model));

                            if (neighbour_distance < closest_distance) {
                                closest_distance = current_distance;
                                closest = neighbour;
                            }
                            if (neighbour_distance > farthest_distance) {
                                farthest_distance = current_distance;
                                farthest = neighbour;
                            }
                        }
                    }
                }
            }
        }

        OctreeRenderIntent rendering_intent{
            .instances_active = instance_active,
            .instances_model_mats = instance_models,
            .instance_count = instance_models.size(),
        };

        if (closest.has_value()) {
            rendering_intent.closest_node = closest;
            rendering_intent.min_scene_distance = std::numeric_limits<double>::infinity();
            for (glm::dvec3 closest_corner : radix::geometry::corners(get_node_bounds(closest.value()))) {
                double dist = glm::distance(closest_corner, cam_pos);

                if (dist < rendering_intent.min_scene_distance) {
                    rendering_intent.min_scene_distance = dist;
                }
            }
        }
        if (farthest.has_value()) {
            rendering_intent.max_scene_distance = 0;
            for (glm::dvec3 farthest_corner : radix::geometry::corners(get_node_bounds(farthest.value()))) {
                double dist = glm::distance(farthest_corner, cam_pos);

                if (dist > rendering_intent.max_scene_distance) {
                    rendering_intent.max_scene_distance = dist;
                }
            }
        }

        return rendering_intent;
    }

private:
};

} // namespace octree
