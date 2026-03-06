#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Vector2D.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
struct SplitByVertexResult {
    std::vector<mesh::Simple_<n_dims, T>> groups;
    std::vector<uint32_t> vertex_remap;
};

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group);
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByVertexResult<n_dims, T> split_by_vertex(const mesh::Simple_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&vertex_to_group);

template <glm::length_t n_dims, typename T>
struct SplitByTriangleResult {
    std::vector<mesh::Simple_<n_dims, T>> groups;
    Vector2D<uint32_t> vertex_remap;
};

template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle(const mesh::View_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group);
template <glm::length_t n_dims, typename T, typename Mapping>
SplitByTriangleResult<n_dims, T> split_by_triangle(const mesh::Simple_<n_dims, T> &mesh, const uint32_t group_count, Mapping &&triangle_to_group);

} // namespace mesh

#include "split.inl"
