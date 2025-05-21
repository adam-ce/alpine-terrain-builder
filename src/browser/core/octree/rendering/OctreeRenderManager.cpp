#include "OctreeRenderManager.h"

namespace octree {
    OctreeRenderManager::OctreeRenderManager(octree::Space space) : m_space(space) {
    }


    OctreeRenderIntent OctreeRenderManager::generate_octree_render_intent(const Id root, glm::dvec3 cam_pos, bool draw_neighbours_only, float refining_ratio = 1.0f) {
        // refining_ratio defines under which ratio between dist(cam, node_bbox_centre) / node_bbox_size the node is split
        // I.e.: dist = 2000 and node_bbox_size = 4000 -> ratio = 0.5 -> node would get refined

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

            Bounds current_bounds = m_space.get_node_bounds(current);

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
                    glm::dmat4 model_translate = glm::translate(glm::dmat4(1.0f), current_bounds.centre() - cam_pos);
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
                        glm::dmat4 model_translate = glm::translate(glm::dmat4(1.0f), current_bounds.centre() - cam_pos);
                        glm::mat4 model = model_translate * model_scale;

                        instance_active.push_back(1.0f);
                        instance_models.push_back(glm::mat4(model));

                        for (auto neighbour : current.neighbours()) {
                            Bounds neighbour_bounds = m_space.get_node_bounds(neighbour);

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
            for (glm::dvec3 closest_corner : radix::geometry::corners(m_space.get_node_bounds(closest.value()))) {
                double dist = glm::distance(closest_corner, cam_pos);

                if (dist < rendering_intent.min_scene_distance) {
                    rendering_intent.min_scene_distance = dist;
                }
            }
        }
        if (farthest.has_value()) {
            rendering_intent.max_scene_distance = 0;
            for (glm::dvec3 farthest_corner : radix::geometry::corners(m_space.get_node_bounds(farthest.value()))) {
                double dist = glm::distance(farthest_corner, cam_pos);

                if (dist > rendering_intent.max_scene_distance) {
                    rendering_intent.max_scene_distance = dist;
                }
            }
        }

        return rendering_intent;
    }
}