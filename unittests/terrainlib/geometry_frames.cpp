#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "PlaneFrame.h"
#include "geometry_utils.h"

namespace {

// A triangle in no particular plane, so nothing lines up with an axis by accident.
radix::geometry::Triangle<3, double> make_tilted_triangle() {
    return {
        glm::dvec3(1.0, -2.0, 3.0),
        glm::dvec3(4.0, 5.0, -1.0),
        glm::dvec3(-2.0, 3.0, 7.0),
    };
}

radix::geometry::Triangle<2, double> make_right_triangle() {
    return {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(4.0, 0.0),
        glm::dvec2(0.0, 4.0),
    };
}

constexpr double tolerance = 1e-12;

} // namespace

TEST_CASE("PlaneFrame::from_triangle spans the triangle's plane", "[terrainlib][frames]") {
    const radix::geometry::Triangle<3, double> triangle = make_tilted_triangle();
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle);

    REQUIRE(frame.has_value());
    CHECK(glm::distance(frame->normal, radix::geometry::normal(triangle)) < tolerance);
    CHECK(std::abs(glm::length(frame->tangent) - 1.0) < tolerance);
    CHECK(std::abs(glm::length(frame->bitangent) - 1.0) < tolerance);
    CHECK(std::abs(glm::dot(frame->tangent, frame->bitangent)) < tolerance);
    CHECK(std::abs(glm::dot(frame->tangent, frame->normal)) < tolerance);
    CHECK(std::abs(glm::dot(frame->bitangent, frame->normal)) < tolerance);
}

TEST_CASE("PlaneFrame::from_triangle rejects a degenerate triangle", "[terrainlib][frames]") {
    const radix::geometry::Triangle<3, double> collinear = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 1.0, 1.0),
        glm::dvec3(2.0, 2.0, 2.0),
    };
    CHECK_FALSE(PlaneFrame::from_triangle(collinear).has_value());

    const radix::geometry::Triangle<3, double> repeated = {
        glm::dvec3(3.0, 3.0, 3.0),
        glm::dvec3(3.0, 3.0, 3.0),
        glm::dvec3(5.0, 1.0, 0.0),
    };
    CHECK_FALSE(PlaneFrame::from_triangle(repeated).has_value());
}

TEST_CASE("CoordFrame maps to local coordinates and back", "[terrainlib][frames]") {
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(make_tilted_triangle());
    REQUIRE(frame.has_value());

    const glm::dvec3 position(7.0, -3.0, 2.0);
    CHECK(glm::distance(frame->to_world(frame->to_local(position)), position) < tolerance);

    // The origin sits at the local origin, and the corners lie in the plane.
    CHECK(glm::length(frame->to_local(frame->origin)) < tolerance);
    for (const glm::dvec3 &corner : make_tilted_triangle()) {
        CHECK(std::abs(frame->distance_to(corner)) < tolerance);
    }
}

TEST_CASE("PlaneFrame separates the in-plane position from the height", "[terrainlib][frames]") {
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(make_tilted_triangle());
    REQUIRE(frame.has_value());

    const glm::dvec3 raised = frame->origin + 2.5 * frame->normal;
    CHECK(std::abs(frame->distance_to(raised) - 2.5) < tolerance);
    CHECK(glm::length(frame->project(raised)) < tolerance);

    const glm::dvec3 sunk = frame->origin - 1.5 * frame->normal;
    CHECK(std::abs(frame->distance_to(sunk) + 1.5) < tolerance);
}

TEST_CASE("PlaneFrame::flatten lays a triangle out counter-clockwise", "[terrainlib][frames]") {
    const radix::geometry::Triangle<3, double> triangle = make_tilted_triangle();
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle);
    REQUIRE(frame.has_value());

    const radix::geometry::Triangle<2, double> flat = frame->flatten(triangle);
    CHECK(cross_2d(flat[1] - flat[0], flat[2] - flat[0]) > 0.0);
    // Flattening preserves lengths, since the frame is orthonormal and the triangle lies in its plane.
    CHECK(std::abs(glm::distance(flat[0], flat[1]) - glm::distance(triangle[0], triangle[1])) < tolerance);
    CHECK(std::abs(glm::distance(flat[1], flat[2]) - glm::distance(triangle[1], triangle[2])) < tolerance);
}

TEST_CASE("distance_to_segment clamps to the segment's ends", "[terrainlib][geometry]") {
    const radix::geometry::Edge<2, double> segment = {glm::dvec2(0.0, 0.0), glm::dvec2(4.0, 0.0)};

    CHECK(distance_to_segment(glm::dvec2(2.0, 3.0), segment) == Catch::Approx(3.0));
    CHECK(distance_to_segment(glm::dvec2(-3.0, 0.0), segment) == Catch::Approx(3.0));
    CHECK(distance_to_segment(glm::dvec2(9.0, 0.0), segment) == Catch::Approx(5.0));
    CHECK(distance_to_segment(glm::dvec2(1.0, 0.0), segment) == Catch::Approx(0.0));

    const radix::geometry::Edge<2, double> degenerate = {glm::dvec2(1.0, 1.0), glm::dvec2(1.0, 1.0)};
    CHECK(distance_to_segment(glm::dvec2(1.0, 4.0), degenerate) == Catch::Approx(3.0));
}

TEST_CASE("distance_to_triangle is zero inside and grows outside", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> triangle = make_right_triangle();

    CHECK(distance_to_triangle(glm::dvec2(1.0, 1.0), triangle) == 0.0);
    CHECK(distance_to_triangle(glm::dvec2(0.0, 0.0), triangle) == 0.0);
    CHECK(distance_to_triangle(glm::dvec2(2.0, 0.0), triangle) == 0.0);
    CHECK(distance_to_triangle(glm::dvec2(0.0, -3.0), triangle) == Catch::Approx(3.0));
    CHECK(distance_to_triangle(glm::dvec2(-2.0, 2.0), triangle) == Catch::Approx(2.0));
}

TEST_CASE("distance_to_triangle accepts either winding", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> counter_clockwise = make_right_triangle();
    const radix::geometry::Triangle<2, double> clockwise = {
        counter_clockwise[2],
        counter_clockwise[1],
        counter_clockwise[0],
    };

    CHECK(distance_to_triangle(glm::dvec2(1.0, 1.0), clockwise) == 0.0);
    CHECK(distance_to_triangle(glm::dvec2(0.0, -3.0), clockwise) == Catch::Approx(3.0));
}

TEST_CASE("triangles_overlap counts touching as overlapping", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> triangle = make_right_triangle();

    CHECK(triangles_overlap(triangle, triangle));

    // Shifted so the two share only the corner at (4, 0).
    const radix::geometry::Triangle<2, double> touching = {
        glm::dvec2(4.0, 0.0),
        glm::dvec2(8.0, 0.0),
        glm::dvec2(4.0, -4.0),
    };
    CHECK(triangles_overlap(triangle, touching));

    const radix::geometry::Triangle<2, double> apart = {
        glm::dvec2(100.0, 100.0),
        glm::dvec2(104.0, 100.0),
        glm::dvec2(100.0, 104.0),
    };
    CHECK_FALSE(triangles_overlap(triangle, apart));

    // Contained entirely within the first, so no edge crosses any other.
    const radix::geometry::Triangle<2, double> inner = {
        glm::dvec2(0.5, 0.5),
        glm::dvec2(1.5, 0.5),
        glm::dvec2(0.5, 1.5),
    };
    CHECK(triangles_overlap(triangle, inner));
}