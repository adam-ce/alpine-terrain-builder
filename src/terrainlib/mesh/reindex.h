#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
void reindex_inplace(mesh::Simple_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::View_<n_dims, T> &mesh);
template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::Simple_<n_dims, T> &mesh);

}

#include "reindex.inl"
