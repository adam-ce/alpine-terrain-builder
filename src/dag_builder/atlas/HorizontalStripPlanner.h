#pragma once

#include <span>

#include <glm/glm.hpp>

#include "atlas/Plan.h"
#include "atlas/Planner.h"

namespace atlas {

// Packs textures left-to-right in a single row.
class HorizontalStripPlanner final : public Planner {
public:
    Plan plan(const std::span<const glm::uvec2> texture_sizes) const override;
};

} // namespace atlas
