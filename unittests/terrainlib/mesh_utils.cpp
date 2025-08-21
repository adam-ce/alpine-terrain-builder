#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/utils.h"

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

TEST_CASE("reindex_mesh") {
    using Catch::Matchers::UnorderedEquals;

    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 1.0, 1.0),
        glm::dvec3(2.0, 2.0, 2.0),
        glm::dvec3(3.0, 3.0, 3.0),
        glm::dvec3(4.0, 4.0, 4.0)};
    mesh.uvs = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(1.0, 1.0),
        glm::dvec2(2.0, 2.0),
        glm::dvec2(3.0, 3.0),
        glm::dvec2(4.0, 4.0)};
    mesh.triangles = {
        glm::uvec3(1, 4, 3),
        glm::uvec3(1, 0, 3),
    };
    mesh.texture = cv::Mat3b(100, 100);

    auto run_checks = [&](SimpleMesh &original, SimpleMesh &reindexed) {
        CHECK(original.positions != reindexed.positions);
        CHECK(original.triangles != reindexed.triangles);

        CHECK(reindexed.positions.size() == 4);
        CHECK(reindexed.uvs.size() == 4);
        CHECK(reindexed.triangles.size() == original.triangles.size());

        for (uint32_t i = 0; i < original.triangles.size(); i++) {
            const glm::uvec3 &triangle = original.triangles[i];
            const glm::uvec3 &reindexed_triangle = reindexed.triangles[i];
            for (uint32_t j = 0; j < 3; j++) {
                const uint32_t original_index = triangle[j];
                const uint32_t new_index = reindexed_triangle[j];
                CHECK(original.positions[original_index] == reindexed.positions[new_index]);
                CHECK(original.uvs[original_index] == reindexed.uvs[new_index]);
            }
        }
        CHECK_THAT(reindexed.positions, UnorderedEquals<glm::dvec3>({glm::dvec3(0.0, 0.0, 0.0),
                                                                     glm::dvec3(1.0, 1.0, 1.0),
                                                                     glm::dvec3(3.0, 3.0, 3.0),
                                                                     glm::dvec3(4.0, 4.0, 4.0)}));
        CHECK_THAT(reindexed.uvs, UnorderedEquals<glm::dvec2>({glm::dvec2(0.0, 0.0),
                                                               glm::dvec2(1.0, 1.0),
                                                               glm::dvec2(3.0, 3.0),
                                                               glm::dvec2(4.0, 4.0)}));
        CHECK(reindexed.texture.has_value());
        CHECK(mat_equals(*reindexed.texture, *original.texture));
    };

    SECTION("const SimpleMesh& overload") {
        SimpleMesh reindexed_mesh = reindex_mesh(static_cast<const SimpleMesh &>(mesh));
        run_checks(mesh, reindexed_mesh);
    }

    SECTION("non-const SimpleMesh& overload") {
        SimpleMesh original_mesh = mesh;
        reindex_mesh(static_cast<SimpleMesh &>(mesh));
        SimpleMesh reindexed_mesh = std::move(mesh);
        run_checks(original_mesh, reindexed_mesh);
    }
}

TEST_CASE("mesh::find_boundary_edges") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        auto edges = find_boundary_edges(mesh);

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

        auto edges = find_boundary_edges(mesh);

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

        auto edges = find_boundary_edges(mesh);

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

        auto edges = find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected;

        REQUIRE_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }
}
