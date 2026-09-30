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

#include <cstdint>
#include <span>

#include <glm/glm.hpp>

#include "Range.h"

namespace mesh {
    
uint32_t find_min_vertex_index(const std::span<const glm::uvec3> triangles);
uint32_t find_max_vertex_index(const std::span<const glm::uvec3> triangles);
Range<uint32_t> find_vertex_index_range(const std::span<const glm::uvec3> triangles);
uint32_t compute_vertex_count(const std::span<const glm::uvec3> triangles);
uint32_t vertex_buffer_size(const std::span<const glm::uvec3> triangles);

}
