#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "geometry/geometry.h"
#include "polygon/clip.h"

namespace {

using polygon::Triangle2d;

Triangle2d make_triangle(const glm::dvec2 &a, const glm::dvec2 &b, const glm::dvec2 &c) {
    return {a, b, c};
}

std::vector<Triangle2d> clip(const Triangle2d &subject, const Triangle2d &clip_triangle) {
    std::vector<Triangle2d> out;
    polygon::clip_triangle(subject, clip_triangle, out);
    return out;
}

double area_of(const Triangle2d &triangle) {
    return std::abs(geometry::cross(triangle[1] - triangle[0], triangle[2] - triangle[0])) / 2.0;
}

double total_area(const std::vector<Triangle2d> &triangles) {
    double area = 0.0;
    for (const Triangle2d &triangle : triangles) {
        area += area_of(triangle);
    }
    return area;
}

// The lower left half of the unit square.
const Triangle2d unit = make_triangle({0, 0}, {1, 0}, {0, 1});

} // namespace

TEST_CASE("clip_triangle keeps a triangle contained in the clip", "[terrainlib][polygon][clip]") {
    const Triangle2d big = make_triangle({-1, -1}, {3, -1}, {-1, 3});

    const std::vector<Triangle2d> pieces = clip(unit, big);

    REQUIRE(pieces.size() == 1);
    CHECK(total_area(pieces) == Catch::Approx(area_of(unit)));
}

TEST_CASE("clip_triangle drops a triangle outside the clip", "[terrainlib][polygon][clip]") {
    const Triangle2d far_away = make_triangle({5, 5}, {6, 5}, {5, 6});

    CHECK(clip(unit, far_away).empty());
}

TEST_CASE("clip_triangle drops triangles meeting in a point or a segment", "[terrainlib][polygon][clip]") {
    // Shares only the corner at (1, 0).
    const Triangle2d at_corner = make_triangle({1, 0}, {2, 0}, {2, 1});
    CHECK(clip(unit, at_corner).empty());

    // Shares the whole edge from (0, 0) to (1, 0), lying on the other side of it.
    const Triangle2d below_edge = make_triangle({0, 0}, {1, 0}, {0, -1});
    CHECK(clip(unit, below_edge).empty());
}

TEST_CASE("clip_triangle halves a triangle cut by the clip", "[terrainlib][polygon][clip]") {
    // Covers x <= 0.5, reaching far enough that its other two edges cut nothing.
    const Triangle2d left_half = make_triangle({0.5, -10}, {0.5, 10}, {-20, 0});

    const std::vector<Triangle2d> pieces = clip(unit, left_half);

    REQUIRE(!pieces.empty());
    // The unit triangle left of x = 0.5 is a trapezoid of area 3/8.
    CHECK(total_area(pieces) == Catch::Approx(0.375));
}

TEST_CASE("clip_triangle produces a hexagon for a star overlap", "[terrainlib][polygon][clip]") {
    // Two triangles in opposite orientation overlapping in a hexagon.
    const Triangle2d up = make_triangle({0, 0}, {6, 0}, {3, 6});
    const Triangle2d down = make_triangle({0, 4}, {6, 4}, {3, -2});

    const std::vector<Triangle2d> pieces = clip(up, down);

    // A hexagon fans into four triangles.
    REQUIRE(pieces.size() == 4);
    // Corners (2,0) (4,0) (5,2) (4,4) (2,4) (1,2).
    CHECK(total_area(pieces) == Catch::Approx(12.0));
}

TEST_CASE("clip_triangle is insensitive to winding", "[terrainlib][polygon][clip]") {
    const Triangle2d reversed_clip = make_triangle({-1, -1}, {-1, 3}, {3, -1});
    const Triangle2d reversed_subject = {unit[0], unit[2], unit[1]};

    CHECK(total_area(clip(unit, reversed_clip)) == Catch::Approx(area_of(unit)));
    CHECK(total_area(clip(reversed_subject, reversed_clip)) == Catch::Approx(area_of(unit)));
}

TEST_CASE("clip_triangle covers the overlap without counting it twice", "[terrainlib][polygon][clip]") {
    // A generic pair with no shared corners or parallel edges.
    const Triangle2d subject = make_triangle({0, 0}, {4, 1}, {1, 4});
    const Triangle2d clip_triangle = make_triangle({3, 3}, {-1, 2}, {2, -1});

    const std::vector<Triangle2d> pieces = clip(subject, clip_triangle);
    REQUIRE(pieces.size() >= 1);

    // The fan tiles the overlap, so sampling it must hit exactly one piece per point.
    for (const Triangle2d &piece : pieces) {
        const glm::dvec2 centre = (piece[0] + piece[1] + piece[2]) / 3.0;
        REQUIRE(geometry::distance_to_triangle(centre, subject) == 0.0);
        REQUIRE(geometry::distance_to_triangle(centre, clip_triangle) == 0.0);

        uint32_t containing = 0;
        for (const Triangle2d &other : pieces) {
            containing += geometry::distance_to_triangle(centre, other) == 0.0 ? 1 : 0;
        }
        REQUIRE(containing == 1);
    }
}

TEST_CASE("clip_triangle drops a degenerate subject", "[terrainlib][polygon][clip]") {
    const Triangle2d collinear = make_triangle({0, 0}, {1, 1}, {2, 2});
    const Triangle2d big = make_triangle({-1, -1}, {3, -1}, {-1, 3});

    CHECK(total_area(clip(collinear, big)) == Catch::Approx(0.0));
    CHECK(clip(unit, collinear).empty());
}
