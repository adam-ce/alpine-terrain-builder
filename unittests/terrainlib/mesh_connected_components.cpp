#include <glm/glm.hpp>

#include <catch2/catch_test_macros.hpp>

#include "../catch2_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/connected_components.h"

TEST_CASE("find_connected_components on empty mesh") {
    SimpleMesh mesh;
    auto result = mesh::find_connected_components(mesh);

    CHECK(result.vertex_to_component.empty());
    CHECK(result.component_count == 0);
}

TEST_CASE("find_connected_components on single triangle") {
    SimpleMesh mesh;
    mesh.positions = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}};
    mesh.triangles = {glm::uvec3(0, 1, 2)};

    auto result = mesh::find_connected_components(mesh);

    CHECK(result.vertex_to_component.size() == 3);
    CHECK(result.component_count == 1);
    for (auto c : result.vertex_to_component) {
        CHECK(c == 0);
    }
}

TEST_CASE("find_connected_components on two disjoint triangles") {
    SimpleMesh mesh;
    mesh.positions = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0},
        {2.0, 1.0, 0.0}};
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(3, 4, 5)};

    auto result = mesh::find_connected_components(mesh);

    CHECK(result.vertex_to_component.size() == 6);
    CHECK(result.component_count == 2);

    // First triangle vertices share component
    CHECK(result.vertex_to_component[0] == result.vertex_to_component[1]);
    CHECK(result.vertex_to_component[1] == result.vertex_to_component[2]);

    // Second triangle vertices share component
    CHECK(result.vertex_to_component[3] == result.vertex_to_component[4]);
    CHECK(result.vertex_to_component[4] == result.vertex_to_component[5]);

    // The two groups must be different
    CHECK(result.vertex_to_component[0] != result.vertex_to_component[3]);
}

TEST_CASE("split_into_connected_components keeps triangles grouped") {
    SimpleMesh mesh;
    mesh.positions = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0},
        {2.0, 1.0, 0.0}};
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(3, 4, 5)};

    auto components = mesh::split_into_connected_components(mesh);

    CHECK(components.size() == 2);

    // Each component must have exactly one triangle
    CHECK(components[0].triangles.size() == 1);
    CHECK(components[1].triangles.size() == 1);

    // Each component must have 3 vertices
    CHECK(components[0].positions.size() == 3);
    CHECK(components[1].positions.size() == 3);
}

TEST_CASE("split_into_connected_components with shared vertex triangles") {
    SimpleMesh mesh;
    mesh.positions = {
        {0.0, 0.0, 0.0},  // 0
        {1.0, 0.0, 0.0},  // 1
        {0.0, 1.0, 0.0},  // 2
        {1.0, 1.0, 0.0}}; // 3
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 2, 3)};

    auto components = mesh::split_into_connected_components(mesh);

    // Both triangles are connected, so one component
    CHECK(components.size() == 1);
    CHECK(components[0].triangles.size() == 2);
    CHECK(components[0].positions.size() == 4);
}
