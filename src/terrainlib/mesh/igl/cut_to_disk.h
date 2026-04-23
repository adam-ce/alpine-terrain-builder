#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/VertexMap.h"
#include "mesh/View.h"

namespace mesh {

void find_cut_to_disk(const std::span<const glm::uvec3> &triangles, std::vector<std::vector<uint32_t>> &cuts);
inline std::vector<std::vector<uint32_t>> find_cut_to_disk(const std::span<const glm::uvec3> &triangles);
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_cut_to_disk(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_cut_to_disk(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions);
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    std::vector<glm::vec<2, T>> &uvs);
std::vector<uint32_t> cut_to_disk(std::span<glm::uvec3> triangles);
template <typename Reserve, typename Duplicate>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    Reserve &&reserve,
    Duplicate &&duplicate);

} // namespace mesh

#include "cut_to_disk.inl"
