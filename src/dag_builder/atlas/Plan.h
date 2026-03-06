#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Rect.h"

namespace atlas {

struct Plan {
    glm::uvec2 size;
    std::vector<Rect2ui> slots;
};

} // namespace atlas
