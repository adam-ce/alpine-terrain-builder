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

#include <span>

#include <glm/glm.hpp>

#include "atlas/rect/Plan.h"
#include "atlas/rect/Planner.h"

namespace atlas {

// Packs textures using a 2D bin packing algorithm (rectpack2D).
class PackingPlanner final : public Planner {
public:
    Plan plan(const std::span<const glm::uvec2> texture_sizes) const override;
};

} // namespace atlas
