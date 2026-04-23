#pragma once

#include <span>
#include <vector>

#include <glm/common.hpp>

namespace mesh {

void orient_triangles_inplace(const std::span<glm::uvec3> triangles);
std::vector<glm::uvec3> orient_triangles(const std::span<glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
void orient_inplace(mesh::Simple_<n_dims, T> &mesh) {
    orient_triangles_inplace(mesh.triangles);
}

} // namespace mesh

