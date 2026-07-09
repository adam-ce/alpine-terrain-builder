#pragma once

#include <glm/glm.hpp>

#include "pch.h"

namespace convert {

namespace {
// Helper to detect if Point has a member function `.z()`
template <typename T, typename = void>
struct has_z : std::false_type {};

template <typename T>
struct has_z<T, std::void_t<decltype(std::declval<T>().z())>> : std::true_type {};
}

template <typename Point2,
          std::enable_if_t<!has_z<Point2>::value, int> = 0>
glm::dvec2 to_glm_point(const Point2 &point) {
    return glm::dvec2(
        CGAL::to_double(point.x()),
        CGAL::to_double(point.y()));
}
template <typename Point3,
          std::enable_if_t<has_z<Point3>::value, int> = 0>
glm::dvec3 to_glm_point(const Point3 &point) {
    return glm::dvec3(
        CGAL::to_double(point.x()),
        CGAL::to_double(point.y()),
        CGAL::to_double(point.z()));
}


template <typename Kernel>
typename Kernel::Point_3 to_cgal_point(const glm::dvec3 &point) {
    return typename Kernel::Point_3(point.x, point.y, point.z);
}
template <typename Kernel>
typename Kernel::Point_2 to_cgal_point(const glm::dvec2 &point) {
    return typename Kernel::Point_2(point.x, point.y);
}

cgal::Mesh to_cgal_mesh(const SimpleMesh& mesh);
SimpleMesh to_simple_mesh(const cgal::Mesh& cgal_mesh);

}
