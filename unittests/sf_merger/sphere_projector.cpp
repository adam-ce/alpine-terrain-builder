#include "../catch2_helpers.h"

#include <catch2/catch_approx.hpp>
#include <glm/glm.hpp>
#include <random>

#include "SphereProjector.h"
#include "log.h"

static constexpr double epsilon = 1e-6;
inline void check_equal(const glm::dvec2& a, const glm::dvec2& b) {
    CHECK(a.x == Catch::Approx(b.x).margin(epsilon));
    CHECK(a.y == Catch::Approx(b.y).margin(epsilon));
}
inline void check_equal(const glm::dvec3& a, const glm::dvec3& b) {
    CHECK(a.x == Catch::Approx(b.x).margin(epsilon));
    CHECK(a.y == Catch::Approx(b.y).margin(epsilon));
    CHECK(a.z == Catch::Approx(b.z).margin(epsilon));
}

TEST_CASE("SphereProjector: roundtrip for standard origin") {
    const double radius = 10.0;
    const glm::dvec3 tangent_point(radius, 0.0, 0.0);
    const SphereProjector projector(tangent_point);

    const std::array<glm::dvec3, 5> test_points = {
        glm::dvec3{ radius,  0.0,    0.0},
        glm::dvec3{ 0.0,     radius, 0.0},
        glm::dvec3{ 0.0,     0.0,    radius},
        glm::dvec3{-radius,  0.0,    0.0},
        glm::dvec3{ 0.0,    -radius, 0.0}
    };

    for (const auto& point : test_points) {
        INFO(fmt::format("point: {}", point));
        const auto projected = projector.project_point(point);
        INFO(fmt::format("projected: {}", projected));
        const auto unprojected = projector.unproject_point(projected);
        INFO(fmt::format("unprojected: {}", unprojected));

        check_equal(unprojected, point);
        CHECK(glm::length(unprojected) == Catch::Approx(radius).margin(epsilon));
    }
}

TEST_CASE("SphereProjector: roundtrip for non-standard origin") {
    const glm::dvec3 tangent_point(10, 5, 6);
    const double radius = glm::length(tangent_point);
    const SphereProjector projector(tangent_point);

    const std::array<glm::dvec3, 5> test_points = {
        glm::dvec3{ radius,  0.0,    0.0},
        glm::dvec3{ 0.0,     radius, 0.0},
        glm::dvec3{ 0.0,     0.0,    radius},
        glm::dvec3{-radius,  0.0,    0.0},
        glm::dvec3{ 0.0,    -radius, 0.0}
    };

    for (const auto& point : test_points) {
        INFO(fmt::format("point: {}", point));
        const auto projected = projector.project_point(point);
        INFO(fmt::format("projected: {}", projected));
        const auto unprojected = projector.unproject_point(projected);
        INFO(fmt::format("unprojected: {}", unprojected));

        check_equal(unprojected, point);
        CHECK(glm::length(unprojected) == Catch::Approx(radius).margin(epsilon));
    }
}
