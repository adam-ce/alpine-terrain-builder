#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "geometry/Rect.h"

namespace atlas {

struct Layout {
    struct Slot {
        size_t mesh_index;
        glm::uvec2 pixel_position;
        Rect2d uv_space;
    };

    glm::uvec2 atlas_size;
    std::vector<Slot> slots;
};

}
