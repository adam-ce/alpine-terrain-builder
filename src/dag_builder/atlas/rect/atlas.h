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
