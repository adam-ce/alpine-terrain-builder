#pragma once

#include <span>

#include <glm/glm.hpp>

#include "atlas/Plan.h"
#include "atlas/Planner.h"

namespace atlas {

// Packs textures using a 2D bin packing algorithm (rectpack2D).
class PackingPlanner final : public Planner {
public:
    Plan plan(const std::span<const glm::uvec2> texture_sizes) const override;
};

} // namespace atlas
