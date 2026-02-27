#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh);
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles);

template <typename T>
void remove_duplicate_triangles(mesh::Simple_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions);
template <typename T>
void remove_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh);
void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3> &triangles);

template <glm::length_t n_dims, typename T>
size_t remove_isolated_vertices(SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T, typename Size = float>
size_t remove_triangles_of_negligible_size(
    SimpleMesh_<n_dims, T> &mesh,
    const Size threshold_percentage_of_average);

bool is_degenerate(const glm::uvec3 &triangle);
void remove_degenerate_triangles(std::vector<glm::uvec3> &triangles);

#include "cleanup.inl"
