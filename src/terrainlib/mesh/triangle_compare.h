#pragma once

#include <glm/glm.hpp>

namespace mesh {

constexpr bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2);
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2);

} // namespace mesh

#include "triangle_compare.inl"