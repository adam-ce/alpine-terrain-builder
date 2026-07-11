#include "../catch2_helpers.h"
#include "mesh/combine.h"
#include "mesh/TriangleSoup.h"

TEST_CASE("mesh::combine_inplace") {
    SECTION("triangle indices are offset by the original vertex count") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));
        mesh1.positions.push_back(glm::dvec3(0, 1, 0));
        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(2, 0, 0));
        mesh2.positions.push_back(glm::dvec3(3, 0, 0));
        mesh2.positions.push_back(glm::dvec3(2, 1, 0));
        mesh2.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh expected;
        expected.positions.push_back(glm::dvec3(0, 0, 0));
        expected.positions.push_back(glm::dvec3(1, 0, 0));
        expected.positions.push_back(glm::dvec3(0, 1, 0));
        expected.positions.push_back(glm::dvec3(2, 0, 0));
        expected.positions.push_back(glm::dvec3(3, 0, 0));
        expected.positions.push_back(glm::dvec3(2, 1, 0));
        expected.triangles.push_back(glm::uvec3(0, 1, 2));
        expected.triangles.push_back(glm::uvec3(3, 4, 5));

        mesh::combine_inplace(mesh1, mesh2);

        CHECK(to_sorted_triangle_soup(mesh1) == to_sorted_triangle_soup(expected));
    }
}