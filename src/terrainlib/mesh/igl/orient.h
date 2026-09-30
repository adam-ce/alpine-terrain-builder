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
#include <vector>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"

namespace mesh {

void orient_triangles_inplace(const std::span<glm::uvec3> triangles);
std::vector<glm::uvec3> orient_triangles(const std::span<glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
void orient_inplace(mesh::Simple_<n_dims, T> &mesh) {
    orient_triangles_inplace(mesh.triangles);
}

} // namespace mesh

