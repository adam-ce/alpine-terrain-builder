#pragma once

#include <span>

#include <glm/glm.hpp>

#include "atlas/rect/Plan.h"

namespace atlas {

class Planner {
public:
    virtual ~Planner() = default;
    virtual Plan plan(const std::span<const glm::uvec2> texture_sizes) const = 0;
};

} // namespace atlas
