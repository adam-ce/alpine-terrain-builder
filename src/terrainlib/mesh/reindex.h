#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/VertexMap.h"

namespace mesh {

struct TrianglesAndMap {
    std::vector<glm::uvec3> triangles;
    VertexMap remap;
};

template <glm::length_t n_dims, typename T>
struct MeshAndMap {
    mesh::Simple_<n_dims, T> mesh;
    VertexMap remap;
};

// Builds a compact reindexing map based on first vertex encounter order
VertexMap create_reindex_map(std::span<const glm::uvec3> triangles);

void reindex_inplace(std::span<glm::uvec3> triangles);
VertexMap reindex_inplace_with_map(std::span<glm::uvec3> triangles);
template <glm::length_t n_dims, typename T>

void reindex_inplace(mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
VertexMap reindex_inplace_with_map(mesh::Simple_<n_dims, T> &mesh);

std::vector<glm::uvec3> reindex(std::span<const glm::uvec3> triangles);
TrianglesAndMap reindex_with_map(std::span<const glm::uvec3> triangles);

template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::View_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
MeshAndMap<n_dims, T> reindex_with_map(const mesh::View_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::Simple_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
MeshAndMap<n_dims, T> reindex_with_map(const mesh::Simple_<n_dims, T> &mesh);


} // namespace mesh

#include "reindex.inl"