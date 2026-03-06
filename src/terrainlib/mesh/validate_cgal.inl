#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/validate_cgal.h"

namespace cgal {

template <typename Point>
inline void validate(const CGAL::Surface_mesh<Point> &mesh) {
#ifndef NDEBUG
    DEBUG_ASSERT(mesh.is_valid()); 
    DEBUG_ASSERT(CGAL::is_triangle_mesh(mesh));
    DEBUG_ASSERT(CGAL::is_valid_polygon_mesh(mesh));
    DEBUG_ASSERT(!CGAL::Polygon_mesh_processing::does_self_intersect(mesh));
#else
    USE(mesh);
#endif
}
}
