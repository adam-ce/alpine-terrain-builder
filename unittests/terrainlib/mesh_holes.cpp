#include <glm/glm.hpp>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtx/hash.hpp>

#include "../catch2_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/holes.h"
#include "mesh/validate.h"
#include "mesh/boundary.h"
#include "log_impls.h"

std::string vec2d_to_string(const std::vector<std::vector<uint32_t>> &v) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < v.size(); ++i) {
        oss << "{";
        for (size_t j = 0; j < v[i].size(); ++j) {
            oss << v[i][j];
            if (j + 1 < v[i].size())
                oss << ", ";
        }
        oss << "}";
        if (i + 1 < v.size())
            oss << ", ";
    }
    oss << "}";
    return oss.str();
}

TEST_CASE("find_boundaries returns empty for empty mesh") {
    SimpleMesh mesh;
    auto boundaries = mesh::find_boundaries(mesh);
    CHECK(boundaries.empty());
}

TEST_CASE("find_boundaries returns single boundary of all vertices for single triangle") {
    SimpleMesh mesh;
    mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh.triangles = {glm::uvec3(0, 1, 2)};

    auto boundaries = mesh::find_boundaries(mesh);

    INFO(vec2d_to_string(boundaries));
    CHECK(boundaries.size() == 1);
    std::vector<uint32_t> expected = {0, 1, 2};
    CHECK_THAT(boundaries[0], Catch::Matchers::UnorderedEquals(expected));
}

TEST_CASE("find_boundaries returns single boundary of all vertices for two connected triangle") {
    SimpleMesh mesh;
    mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 1, 1}};
    mesh.triangles = {glm::uvec3(0, 1, 3), glm::uvec3(1, 2, 3)};

    auto boundaries = mesh::find_boundaries(mesh);

    std::vector<uint32_t> expected = {0, 3, 2, 1};
    CHECK(boundaries.size() == 1);
    auto boundary = boundaries[0];
    mesh::normalize_face_index_rotation(boundary, true);
    CHECK_THAT(boundary, Catch::Matchers::UnorderedEquals(expected));
}

TEST_CASE("find_boundaries returns two boundaries for two isolated triangles") {
    SimpleMesh mesh;
    mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 2, 1}, {0, 1, 2}};
    mesh.triangles = {glm::uvec3(0, 1, 2), glm::uvec3(3, 4, 5)};

    auto boundaries = mesh::find_boundaries(mesh);
    CHECK(boundaries.size() == 2);
    if (boundaries[0][0] > boundaries[1][0]) {
        std::swap(boundaries[0], boundaries[1]);
    }
    std::vector<uint32_t> expected1 = {0, 1, 2};
    CHECK_THAT(boundaries[0], Catch::Matchers::UnorderedEquals(expected1));
    std::vector<uint32_t> expected2 = {3, 4, 5};
    CHECK_THAT(boundaries[1], Catch::Matchers::UnorderedEquals(expected2));
}

TEST_CASE("find_holes find square hole") {
    SimpleMesh mesh;
    // Outer square: 0,1,2,3
    // Inner hole square: 4,5,6,7
    mesh.positions = {
        {0, 0, 0},     // 0
        {2, 0, 0},     // 1
        {2, 2, 0},     // 2
        {0, 2, 0},     // 3
        {0.5, 0.5, 0}, // 4
        {1.5, 0.5, 0}, // 5
        {1.5, 1.5, 0}, // 6
        {0.5, 1.5, 0}  // 7
    };

    // Triangles forming a frame (outer square minus inner square)
    mesh.triangles = {
        // bottom strip
        {0, 1, 5},
        {0, 5, 4},
        // right strip
        {1, 2, 6},
        {1, 6, 5},
        // top strip
        {2, 3, 7},
        {2, 7, 6},
        // left strip
        {3, 0, 4},
        {3, 4, 7}};

    std::vector<uint32_t> outer_loop = {0, 1, 2, 3};
    std::vector<uint32_t> inner_loop = {4, 5, 6, 7};

    auto boundaries = mesh::find_boundaries(mesh);
    CHECK(boundaries.size() == 2);
    if (boundaries[0][0] > boundaries[1][0]) {
        std::swap(boundaries[0], boundaries[1]);
    }
    CHECK_THAT(boundaries[0], Catch::Matchers::UnorderedEquals(outer_loop));
    CHECK_THAT(boundaries[1], Catch::Matchers::UnorderedEquals(inner_loop));

    auto holes = mesh::find_holes(mesh);
    CHECK(holes.size() == 1);
    CHECK_THAT(holes[0], Catch::Matchers::UnorderedEquals(inner_loop));
}

TEST_CASE("fill_planar_hole fill square hole") {
    SimpleMesh mesh;
    // Outer square: 0,1,2,3
    // Inner hole square: 4,5,6,7
    mesh.positions = {
        {0, 0, 0},     // 0
        {2, 0, 0},     // 1
        {2, 2, 0},     // 2
        {0, 2, 0},     // 3
        {0.5, 0.5, 0}, // 4
        {1.5, 0.5, 0}, // 5
        {1.5, 1.5, 0}, // 6
        {0.5, 1.5, 0}  // 7
    };

    // Triangles forming a frame (outer square minus inner square)
    mesh.triangles = {
        // bottom strip
        {0, 1, 5},
        {0, 5, 4},
        // right strip
        {1, 2, 6},
        {1, 6, 5},
        // top strip
        {2, 3, 7},
        {2, 7, 6},
        // left strip
        {3, 0, 4},
        {3, 4, 7}};

    const std::vector<uint32_t> outer_loop = {0, 1, 2, 3};
    const std::vector<uint32_t> inner_loop = {7, 6, 5, 4}; // {4, 5, 6, 7};

    const auto holes = mesh::find_holes(mesh);
    CHECK(holes.size() == 1);
    CHECK_THAT(holes[0], Catch::Matchers::UnorderedEquals(inner_loop));

    const size_t before = mesh.triangles.size();
    mesh::fill_planar_hole(mesh, inner_loop);
    const size_t after = mesh.triangles.size();

    // two new triangles
    CHECK(after == before + 2);

    // all new triangles only use inner_loop vertices
    for (size_t i = before; i < after; i++) {
        const glm::uvec3 triangle = mesh.triangles[i];
        for (size_t k = 0; k < 3; k++) {
            CHECK(std::find(inner_loop.begin(), inner_loop.end(), triangle[k]) != inner_loop.end());
        }
    }

    auto triangle_normal = [&](const glm::uvec3 &triangle) {
        const auto &a = mesh.positions[triangle[0]];
        const auto &b = mesh.positions[triangle[1]];
        const auto &c = mesh.positions[triangle[2]];
        return glm::normalize(glm::cross(b - a, c - a));
    };
    const glm::dvec3 expected_normal = triangle_normal(mesh.triangles[0]);
    for (const glm::uvec3 triangle : mesh.triangles) {
        const glm::dvec3 actual_normal = triangle_normal(triangle);
        CHECK(expected_normal == actual_normal);
    }
}

TEST_CASE("square frame with two holes sharing a vertex") {
    SimpleMesh mesh;
    mesh.positions = {
        {0, 0, 0},       // 0
        {1, 0, 0},       // 1
        {1, 1, 0},       // 2
        {0, 1, 0},       // 3
        {0.25, 0.25, 0}, // 4
        {0.75, 0.25, 0}, // 5
        {0.75, 0.75, 0}, // 6
        {0.25, 0.75, 0}, // 7
        {0.5, 0.5, 0}    // 8
    };
    mesh.triangles = {
        // bottom strip
        {0, 1, 5},
        {0, 5, 4},
        // right strip
        {1, 2, 6},
        {1, 6, 5},
        // top strip
        {2, 3, 7},
        {2, 7, 6},
        // left strip
        {3, 0, 4},
        {3, 4, 7},
        // top inner
        {6, 7, 8},
        // bottom inner
        {4, 5, 8}
    };
    mesh::validate_connected(mesh);

    SECTION("find_boundary_edges") {
        // expected
        const std::vector<glm::uvec2> expected_edges = {
            // outer loop
            {0, 1},
            {1, 2},
            {2, 3},
            {3, 0},
            // inner hole left
            {4, 7},
            {8, 4},
            {7, 8},
            // inner hole right
            {6, 5},
            {5, 8},
            {8, 6}};
        const std::unordered_set<glm::uvec2> expected_edges_set(expected_edges.begin(), expected_edges.end());
        
        // actual
        const std::unordered_set<glm::uvec2> actual_edges_set = find_boundary_edges(mesh);
        std::vector<glm::uvec2> actual_edges(actual_edges_set.begin(), actual_edges_set.end());
        
        // missing edges
        std::vector<glm::uvec2> missing;
        for (auto const& e : expected_edges) {
            if (actual_edges_set.find(e) == actual_edges_set.end()) {
                missing.push_back(e);
            }
        }

        // unexpected edges
        std::vector<glm::uvec2> unexpected;
        for (auto const& e : actual_edges) {
            if (expected_edges_set.find(e) == expected_edges_set.end()) {
                unexpected.push_back(e);
            }
        }

        INFO("Missing edges: " + fmt::format("{}", fmt::join(missing, ", ")));
        INFO("Unexpected edges: " + fmt::format("{}", fmt::join(unexpected, ", ")));

        REQUIRE_THAT(actual_edges, Catch::Matchers::UnorderedEquals(expected_edges));
    }

    SECTION("find_boundaries") {
        const auto boundaries = mesh::find_boundaries(mesh);

        const std::vector<glm::uvec4> expected_boundaries = {{0, 3, 2, 1}, {4, 8, 7, UINT_MAX}, {5, 6, 8, UINT_MAX}};
        for (const auto &quad : expected_boundaries) {
            CHECK(mesh::normalize_quad(quad) == quad);
        }

        std::vector<glm::uvec4> actual_boundaries;
        for (const auto &boundary : boundaries) {
            INFO(fmt::format("boundary = [{}]", fmt::join(boundary, ", ")));
            REQUIRE(boundary.size() <= 4);
            glm::uvec4 quad(UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX);
            for (size_t i = 0; i < boundary.size(); i++) {
                quad[i] = boundary[i];
            }
            mesh::normalize_quad_inplace(quad);
            size_t out_index = 0;
            for (size_t i = 0; i < 4; i++) {
                if (quad[i] != UINT_MAX) {
                    quad[out_index] = quad[i];
                    out_index += 1;
                }
            }
            for (size_t i = out_index; i < 4; i++) {
                quad[out_index] = UINT_MAX;
            }
            actual_boundaries.push_back(quad);
        }

        REQUIRE_THAT(actual_boundaries, Catch::Matchers::UnorderedEquals(expected_boundaries));
    }

    const auto holes = mesh::find_holes(mesh);
    SECTION("find_holes") {
        const std::vector<glm::uvec3> expected_holes = {{4, 8, 7}, {5, 6, 8}};
        for (const auto &triangle : expected_holes) {
            CHECK(mesh::normalize_triangle(triangle) == triangle);
        }

        std::vector<glm::uvec3> actual_holes;
        for (const auto &hole : holes) {
            INFO(fmt::format("hole = [{}]", fmt::join(hole, ", ")));
            REQUIRE(hole.size() == 3);
            const glm::uvec3 triangle(hole[0], hole[1], hole[2]);
            actual_holes.push_back(mesh::normalize_triangle(triangle));
        }

        REQUIRE_THAT(actual_holes, Catch::Matchers::UnorderedEquals(expected_holes));
    }
}
