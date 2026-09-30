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

#include <vector>

#include <glm/glm.hpp>

#include "geometry/Rect.h"

namespace atlas {

struct Plan {
    glm::uvec2 size;
    std::vector<Rect2ui> slots;
};

inline void rescale(atlas::Plan& plan, const glm::uvec2& texture_size) {
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
