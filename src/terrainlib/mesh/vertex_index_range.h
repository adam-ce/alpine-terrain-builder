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

}
