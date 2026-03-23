#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/bounds.h"

TEST_CASE("calculate_bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 1.0, 1.0),
        glm::dvec3(-1.0, -1.0, -1.0)};

    const auto bounds = calculate_bounds(mesh);
    CHECK(bounds.min == glm::dvec3(-1.0, -1.0, -1.0));
    CHECK(bounds.max == glm::dvec3(1.0, 1.0, 1.0));
}
