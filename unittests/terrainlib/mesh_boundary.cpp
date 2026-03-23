#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/boundary.h"

TEST_CASE("mesh::find_boundary_edges") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        auto edges = mesh::find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected;

        REQUIRE_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("all edges for single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0.f, 0.f, 0.f},
            {1.f, 0.f, 0.f},
            {0.f, 1.f, 0.f}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        auto edges = mesh::find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected = {
            {0, 1}, {1, 2}, {2, 0}};
        REQUIRE_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0.f, 0.f, 0.f},
            {1.f, 0.f, 0.f},
            {0.f, 1.f, 0.f},
            {1.f, 1.f, 0.f}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        auto edges = mesh::find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected = {
            {0, 1}, {2, 0}, {1, 3}, {3, 2} // shared edge {1,2} not included
        };

        REQUIRE_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("empty for cube mesh") {
        SimpleMesh mesh;
        mesh.positions = {
            {-1, -1, -1}, // 0
            {1, -1, -1},  // 1
            {1, 1, -1},   // 2
            {-1, 1, -1},  // 3
            {-1, -1, 1},  // 4
            {1, -1, 1},   // 5
            {1, 1, 1},    // 6
            {-1, 1, 1}    // 7
        };

        mesh.triangles = {
            {0, 1, 2}, {0, 2, 3}, // front
            {1, 5, 6}, {1, 6, 2}, // right
            {5, 4, 7}, {5, 7, 6}, // back
            {4, 0, 3}, {4, 3, 7}, // left
            {3, 2, 6}, {3, 6, 7}, // top
            {4, 5, 1}, {4, 1, 0}  // bottom
        };

        auto edges = mesh::find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected;

        REQUIRE_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }
}
