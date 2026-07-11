#include <algorithm>
#include <cstdint>
#include <span>

#include <glm/glm.hpp>

#include "atlas/rect/HorizontalStripPlanner.h"

namespace atlas {

Plan HorizontalStripPlanner::plan(std::span<const glm::uvec2> texture_sizes) const {
    Plan out{};

    if (texture_sizes.empty()) {
        out.size = glm::uvec2(0u, 0u);
        out.slots.clear();
        return out;
    }

    out.slots.reserve(texture_sizes.size());

    glm::uvec2 cursor{0u, 0u};
    uint32_t max_height = 0u;

    for (const glm::uvec2 &size : texture_sizes) {
        out.slots.push_back(Rect2ui{
            .position = glm::uvec2(cursor.x, cursor.y),
            .size = glm::uvec2(size.x, size.y),
        });

        cursor.x += static_cast<uint32_t>(size.x);
        max_height = std::max(max_height, static_cast<uint32_t>(size.y));
    }

    // If everything is 0x0, keep atlas 0x0.
    if (cursor.x == 0u && max_height == 0u) {
        out.size = glm::uvec2(0u, 0u);
        return out;
    }

    out.size = glm::uvec2(cursor.x, max_height);
    return out;
}

} // namespace atlas
