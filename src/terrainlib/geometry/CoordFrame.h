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

#include <optional>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <radix/geometry.h>

// An orthonormal frame on a surface.
template <typename T = double>
struct CoordFrame_ {
    glm::vec<3, T> origin;
    glm::vec<3, T> tangent;
    glm::vec<3, T> bitangent;
    glm::vec<3, T> normal;

    // The position in this frame's coordinates.
    glm::vec<3, T> to_local(const glm::vec<3, T> &position) const {
        const glm::vec<3, T> offset = position - this->origin;
        return glm::vec<3, T>(
            glm::dot(offset, this->tangent),
            glm::dot(offset, this->bitangent),
            glm::dot(offset, this->normal));
    }

    // The inverse: a position given in this frame's coordinates, back in world coordinates.
    glm::vec<3, T> to_world(const glm::vec<3, T> &local) const {
        return this->origin
            + local.x * this->tangent
            + local.y * this->bitangent
            + local.z * this->normal;
    }
};

using CoordFrame = CoordFrame_<double>;