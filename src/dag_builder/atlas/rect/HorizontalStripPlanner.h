#pragma once

#include <span>

#include <glm/glm.hpp>

#include "atlas/rect/Plan.h"
#include "atlas/rect/Planner.h"

namespace atlas {

// Packs textures left-to-right in a single row.
class HorizontalStripPlanner final : public Planner {
public:
    Plan plan(const std::span<const glm::uvec2> texture_sizes) const override;
};

} // namespace atlas
