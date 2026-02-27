#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"

std::vector<glm::uvec2> find_non_manifold_edges(const std::span<const glm::uvec3> &triangles);
template <glm::length_t n_dims, typename T>
std::vector<glm::uvec2> find_non_manifold_edges(const mesh::Simple_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_edges(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);
template <typename Duplicate>
void duplicate_non_manifold_edges(
    std::span<glm::uvec3> triangles,
    Duplicate &&duplicate_vertex);

template <glm::length_t n_dims, typename T>
void duplicate_non_manifold_vertices(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename Position>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);
template <typename Duplicate>
void duplicate_non_manifold_vertices(
    std::span<glm::uvec3> triangles,
    const uint32_t vertex_count,
    Duplicate &&duplicate_vertex);

template <glm::length_t n_dims, typename T>
void make_manifold(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename Position>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions);
template <glm::length_t n_dims, typename Position, typename Uv>
void make_manifold(
    std::vector<glm::uvec3> &triangles,
    std::vector<glm::vec<n_dims, Position>> &positions,
    std::vector<glm::vec<2, Uv>> &uvs);

template <glm::length_t n_dims, typename T>
bool is_manifold(const SimpleMesh_<n_dims, T> &mesh);

#include "manifold.inl"
