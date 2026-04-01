#pragma once

#include <vector>
#include <array>
#include <span>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"

template <glm::length_t n_dims, typename T>
using TriangleSoup_ = std::vector<std::array<glm::vec<n_dims, T>, 3>>;
using TriangleSoup = TriangleSoup_<3, double>;

template <glm::length_t n_dims, typename T>
TriangleSoup_<n_dims, T> to_triangle_soup(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
void sort_triangle_soup(TriangleSoup_<n_dims, T> &soup);
template <glm::length_t n_dims, typename T>
TriangleSoup_<n_dims, T> to_sorted_triangle_soup(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<TriangleSoup_<n_dims, T>> to_sorted_triangle_soups(const std::span<const mesh::Simple_<n_dims, T>> meshes);

#include "TriangleSoup.inl"
