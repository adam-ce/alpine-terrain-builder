#pragma once

#include "cgal.h"

namespace cgal {

template<typename Point>
inline void validate(const CGAL::Surface_mesh<Point> &mesh);

}

#include "mesh/validate_cgal.inl"
