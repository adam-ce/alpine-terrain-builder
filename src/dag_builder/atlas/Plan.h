#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Rect.h"

namespace atlas {

struct Plan {
    glm::uvec2 size;
    std::vector<Rect2ui> slots;
};

void rescale(atlas::Plan& plan, const glm::uvec2& texture_size) {
    if (glm::all(glm::equal(plan.size, glm::uvec2(0)))) {
        return;
    }

    const glm::dvec2 scale = glm::dvec2(texture_size) / glm::dvec2(plan.size);
    const double final_scale = glm::min(scale.x, scale.y);

    plan.size = texture_size;

    for (auto& slot : plan.slots) {
        slot.position = glm::uvec2(glm::round(glm::dvec2(slot.position) * final_scale));
        slot.size = glm::uvec2(glm::round(glm::dvec2(slot.size) * final_scale));

        // Ensure within bounds
        slot.position = glm::min(slot.position, texture_size);
        slot.size = glm::min(slot.size, texture_size - slot.position);
    }
}

} // namespace atlas
