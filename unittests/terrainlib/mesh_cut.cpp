#include <vector>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <glm/common.hpp>

#include "../catch2_helpers.h"
#include "mesh/TriangleSoup.h"
#include "mesh/connectivity/connected_components.h"
#include "mesh/igl/cut.h"
#include "mesh/igl/cut_to_disk.h"

TEST_CASE("cuts_to_edge_mask single edge") {
    const std::vector<glm::uvec3> triangles = {
        {0, 1, 2},
        {2, 1, 3}
    };

    const std::vector<std::vector<uint32_t>> cuts = {
        {1, 2}
    };

    const auto edge_cut_mask = mesh::cuts_to_edge_mask(cuts, triangles);

    REQUIRE(edge_cut_mask.size() == 2);
    CHECK(edge_cut_mask[0] == glm::bvec3(false, true, false));
    CHECK(edge_cut_mask[1] == glm::bvec3(true, false, false));
}

TEST_CASE("cuts_to_edge_mask multiple edges") {
    const std::vector<glm::uvec3> triangles = {
        {0, 1, 2},
        {2, 1, 3}
    };

    const std::vector<std::vector<uint32_t>> cuts = {
        {0, 1, 3}
    };

    const auto edge_cut_mask = mesh::cuts_to_edge_mask(cuts, triangles);

    REQUIRE(edge_cut_mask.size() == 2);
    CHECK(edge_cut_mask[0] == glm::bvec3(true, false, false));
    CHECK(edge_cut_mask[1] == glm::bvec3(false, true, false));
}

TEST_CASE("cuts_to_edge_mask multiple single edges") {
    const std::vector<glm::uvec3> triangles = {
        {0, 1, 2},
        {2, 1, 3}
    };

    const std::vector<std::vector<uint32_t>> cuts = {
        {0, 1},
        {1, 2},
        {1, 3}
    };

    const auto edge_cut_mask = mesh::cuts_to_edge_mask(cuts, triangles);

    REQUIRE(edge_cut_mask.size() == 2);
    CHECK(edge_cut_mask[0] == glm::bvec3(true, true, false));
    CHECK(edge_cut_mask[1] == glm::bvec3(true, true, false));
}

TEST_CASE("cut shared edge in open quad") {
    mesh::Simple mesh({ 
        {0, 1, 2},
        {2, 1, 3}
    }, {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
    });

    const size_t original_vertex_count = mesh.vertex_count();

    const std::vector<glm::bvec3> edge_cut_mask = {
        glm::bvec3(false, true, false),
        glm::bvec3(true, false, false),
    };

    const auto mapping = mesh::cut(mesh, edge_cut_mask);
    CHECK(mapping.size() == original_vertex_count + 2);

    const auto actual = mesh::split_into_connected_components(mesh);
    const auto actual_soups = to_sorted_triangle_soups(actual);
    const std::vector<std::array<glm::dvec3, 3>> expected_soup1 =
        {{glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 1, 0)}};
    const std::vector<std::array<glm::dvec3, 3>> expected_soup2 =
        {{glm::dvec3(0, 1, 0), glm::dvec3(1, 1, 0), glm::dvec3(1, 0, 0)}};
    CAPTURE(actual_soups);
    REQUIRE(actual.size() == 2);
    CHECK(actual_soups[0] == expected_soup1);
    CHECK(actual_soups[1] == expected_soup2);
}

TEST_CASE("cut shared edge in double-sided triangle") {
    mesh::Simple mesh({
        {0, 1, 2},
        {0, 2, 1}
    }, {
        {0, 0, 0},
        {1, 0, 0},
        {0, 1, 0},
    });

    const size_t original_vertex_count = mesh.vertex_count();

    const std::vector<glm::bvec3> edge_cut_mask = {
        glm::bvec3(true, true, false),
        glm::bvec3(false, true, true),
    };

    const auto mapping = mesh::cut(mesh, edge_cut_mask);
    CHECK(mapping.size() == original_vertex_count + 1);
}
