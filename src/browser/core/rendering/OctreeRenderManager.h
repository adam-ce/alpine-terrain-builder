#pragma once
#include "octree/Id.h"
#include "octree/Space.h"
#include <any>
#include <glm/glm.hpp>

namespace octree
{

    struct OctreeRenderIntent
    {
        std::vector<float> instances_active;
        std::vector<glm::mat4> instances_model_mats;
        size_t instance_count;

        std::optional<double> min_scene_distance;
        std::optional<double> max_scene_distance;

        std::optional<Id> closest_node;
    };

    enum OctreeFilterParamType
    {
        Float,
        Double
    };

    struct OctreeFilterParam
    {
        std::string name;
        OctreeFilterParamType type;
        std::any default_value;
        std::string description;
    };

    struct OctreeFilterDefinition
    {
        std::string name;
        std::string description;
    };

    class OctreeRenderManager
    {
    public:
        OctreeRenderManager(Space space);

        OctreeRenderIntent generate_octree_render_intent(const Id root, glm::dvec3 cam_pos, bool draw_neighbours_only, float refining_ratio);

    private:
        octree::Space m_space;
    };

}