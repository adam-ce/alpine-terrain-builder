#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
void sort_and_normalize_triangles(mesh::Simple_<n_dims, T> &mesh);
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>
void sort_triangles(mesh::Simple_<n_dims, T> &mesh);
void sort_triangles(std::span<glm::uvec3> triangles);

void normalize_face_index_rotation(const std::span<uint32_t> face, const bool keep_orientation);

constexpr glm::uvec2 normalize_edge(glm::uvec2 edge);
constexpr void normalize_edge_inplace(glm::uvec2 &edge);

glm::uvec3 normalize_triangle(glm::uvec3 triangle, const bool keep_orientation = true);
void normalize_triangle_inplace(glm::uvec3 &triangle, const bool keep_orientation = true);
void normalize_triangles_inplace(std::span<glm::uvec3> triangles, const bool keep_orientation = true);
void normalize_triangles_inplace(std::vector<glm::uvec3>& triangles, const bool keep_orientation = true);

glm::uvec4 normalize_quad(glm::uvec4 quad, const bool keep_orientation = true);
void normalize_quad_inplace(glm::uvec4 &quad, const bool keep_orientation = true);

}

#include "normalize.inl"
