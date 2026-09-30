/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adrian Gawor
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
        size_t instance_count = 0;

        std::optional<double> min_scene_distance = std::nullopt;
        std::optional<double> max_scene_distance = std::nullopt;

        std::optional<Id> closest_node = std::nullopt;
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
