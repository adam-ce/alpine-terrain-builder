#pragma once

#include "pch.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
inline void validate(const SimpleMesh_<n_dims, T> &mesh);

template<typename Point>
inline void validate(const CGAL::Surface_mesh<Point> &mesh);

}

#include "mesh/validate.inl"
