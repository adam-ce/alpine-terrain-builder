/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <type_traits>
#include <utility>

#include <glm/glm.hpp>
#include <CGAL/number_utils.h>

#include "mesh/cgal.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

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


template <typename Kernel = cgal::Kernel>
typename Kernel::Point_3 to_cgal_point(const glm::dvec3 &point) {
    return typename Kernel::Point_3(point.x, point.y, point.z);
}
template <typename Kernel = cgal::Kernel>
typename Kernel::Point_2 to_cgal_point(const glm::dvec2 &point) {
    return typename Kernel::Point_2(point.x, point.y);
}

cgal::Mesh to_cgal_mesh(const mesh::View& mesh);
inline cgal::Mesh to_cgal_mesh(const mesh::Simple& mesh) {
    return to_cgal_mesh(mesh::View(mesh));
}
SimpleMesh to_simple_mesh(const cgal::Mesh& cgal_mesh);

}
