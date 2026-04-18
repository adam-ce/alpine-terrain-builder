#pragma once

#include <span>
#include <unordered_set>
#include <vector>

#include <glm/common.hpp>

#include "TriangleContainer.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> get_edges(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
std::unordered_set<glm::uvec2> get_edges(const Triangles &triangles);

template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> get_halfedges(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> get_halfedges(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
std::vector<glm::uvec2> get_halfedges(const Triangles &triangles);

template <glm::length_t n_dims, typename T>
uint32_t compute_edge_count(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
uint32_t compute_edge_count(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
uint32_t compute_edge_count(const Triangles &triangles);

template <glm::length_t n_dims, typename T>
uint32_t compute_halfedge_count(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
uint32_t compute_halfedge_count(const mesh::View_<n_dims, T> &mesh);
template <TriangleContainer Triangles>
uint32_t compute_halfedge_count(const Triangles &triangles);

template <glm::length_t n_dims, typename T, typename F>
void for_each_halfedge(const mesh::Simple_<n_dims, T> &mesh, F &&func, const bool normalize = false);
template <glm::length_t n_dims, typename T, typename F>
void for_each_halfedge(const mesh::View_<n_dims, T> &mesh, F &&func, const bool normalize = false);
template <TriangleContainer Triangles, typename F>
void for_each_halfedge(const Triangles &triangles, F &&func, const bool normalize = false);

template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::Simple_<n_dims, T> &mesh, F &&func);
template <glm::length_t n_dims, typename T, typename F>
void for_each_edge(const mesh::View_<n_dims, T> &mesh, F &&func);
template <TriangleContainer Triangles, typename F>
void for_each_edge(const Triangles &triangles, F &&func);

} // namespace mesh

#include "edges.inl"