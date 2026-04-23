#pragma once

#include <array>
#include <vector>

#include <CGAL/ch_graham_andrew.h>
#include <CGAL/min_quadrilateral_2.h>

#include "mesh/cgal.h"
#include "mesh/convert.h"

namespace uv {

namespace {
template <typename UvMap>
std::vector<cgal::Point2> compute_uv_convex_hull(const UvMap &map, size_t number_of_vertices) {
    std::vector<cgal::Point2> points;
    points.reserve(number_of_vertices);
    for (size_t i = 0; i < number_of_vertices; i++) {
        points.push_back(map[cgal::VertexIndex(i)]);
    }

    std::vector<cgal::Point2> hull;
    hull.reserve(points.size());
    CGAL::ch_graham_andrew(points.begin(), points.end(), std::back_inserter(hull));
    return hull;
}

std::array<cgal::Point2, 4> compute_min_enclosing_rectangle(const std::vector<cgal::Point2> &hull) {
    std::array<cgal::Point2, 4> rectangle{};
    if (hull.size() >= 2) {
        CGAL::min_rectangle_2(hull.begin(), hull.end(), rectangle.begin());
    }
    return rectangle;
}

template <typename UvMap>
void apply_uv_rotation(
    UvMap &map,
    size_t number_of_vertices,
    const std::array<cgal::Point2, 4> &rectangle) {
    using FT = cgal::Kernel::FT;
    using Vector_2 = cgal::Kernel::Vector_2;

    const Vector_2 edge = rectangle[1] - rectangle[0];
    const FT edge_length = CGAL::sqrt(edge.squared_length());
    if (edge_length == FT(0)) {
        return;
    }

    const Vector_2 ex = edge / edge_length;
    const Vector_2 ey(-ex.y(), ex.x());

    for (size_t i = 0; i < number_of_vertices; ++i) {
        const cgal::Point2 uv = map[cgal::VertexIndex(i)];
        const Vector_2 p = uv - CGAL::ORIGIN;
        map[cgal::VertexIndex(i)] = cgal::Point2(p * ex, p * ey);
    }
}
} // namespace

template <typename UvMap>
void rotate_uv_map_to_best_fit_box(UvMap &map, size_t number_of_vertices) {
    if (number_of_vertices <= 1) {
        return;
    }

    const auto hull = compute_uv_convex_hull(map, number_of_vertices);
    if (hull.size() <= 1) {
        return;
    }

    const auto rectangle = compute_min_enclosing_rectangle(hull);
    apply_uv_rotation(map, number_of_vertices, rectangle);
}

} // namespace
