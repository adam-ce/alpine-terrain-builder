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

#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/bounds.h"

TEST_CASE("calculate_bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 1.0, 1.0),
        glm::dvec3(-1.0, -1.0, -1.0)};

    const auto bounds = calculate_bounds(mesh);
    CHECK(bounds.min == glm::dvec3(-1.0, -1.0, -1.0));
    CHECK(bounds.max == glm::dvec3(1.0, 1.0, 1.0));
}
