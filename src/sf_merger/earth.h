/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include <glm/glm.hpp>

namespace earth {
    constexpr double largest_radius() {
        // Chimborazo
        return 6384400;
    }
    constexpr double smallest_radius() {
        // Litke Deep
        return 6351704.3;
    }
    constexpr double radius() {
        return 6371008;
    }
    constexpr glm::dvec2 radius_range() {
        return {smallest_radius(), largest_radius()};
    }
}
