#ifndef CONVERT_H
#define CONVERT_H

#include "pch.h"

namespace convert {

glm::dvec3 cgal2glm(Point3 point);
glm::dvec2 cgal2glm(Point2 point);

Point3 glm2cgal(glm::dvec3 point);
Point2 glm2cgal(glm::dvec2 point);

SurfaceMesh mesh2cgal(const SimpleMesh& mesh);
SimpleMesh cgal2mesh(const SurfaceMesh& cgal_mesh);

glm::uvec2 cv2glm(const cv::Size size);
cv::Size glm2cv(const glm::uvec2 size);

}

#endif
