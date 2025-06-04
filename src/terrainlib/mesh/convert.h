#pragma once

#include <glm/glm.hpp>

#include "cgal.h"
#include "mesh/SimpleMesh.h"

namespace convert {

glm::dvec3 cgal2glm(cgal::Point3 point);
glm::dvec2 cgal2glm(cgal::Point2 point);

cgal::Point3 glm2cgal(glm::dvec3 point);
cgal::Point2 glm2cgal(glm::dvec2 point);

cgal::SurfaceMesh to_cgal_mesh(const SimpleMesh& mesh);
SimpleMesh to_simple_mesh(const cgal::SurfaceMesh& cgal_mesh);

}
