/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <span>
#include <vector>

#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "atlas/rect/Plan.h"
#include "atlas/rect/Planner.h"

using Texture = cv::Mat;

namespace atlas {

Plan plan(const std::span<const glm::uvec2> texture_sizes);
Plan plan(const std::span<const glm::uvec2> texture_sizes, const Planner &planner);

Texture create(const Plan &plan, const std::span<const Texture> textures);

Texture plan_and_create(const std::span<const Texture> textures);
Texture plan_and_create(const std::span<const Texture> textures, const Planner &planner);

void map_uvs(
    const Plan &plan,
    const uint32_t slot_index,
    std::span<glm::dvec2> uvs);

}
