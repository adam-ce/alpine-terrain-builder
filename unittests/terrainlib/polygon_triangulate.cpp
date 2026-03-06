#include "../catch2_helpers.h"

#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/validate.h"
#include "polygon/triangulate.h"


TEST_CASE("triangulate handles minimal input") {
    SimpleMesh3d mesh;

    SECTION("Empty indices") {
        std::vector<uint32_t> indices;
        polygon::triangulate(mesh, indices);
        REQUIRE(mesh.positions.empty());
        REQUIRE(mesh.triangles.empty());
    }

    SECTION("Single vertex") {
        mesh.positions.push_back({0, 0, 0});
        std::vector<uint32_t> indices = {0};
        polygon::triangulate(mesh, indices);
        REQUIRE(mesh.triangles.empty());
    }

    SECTION("Two vertices") {
        mesh.positions.push_back({0, 0, 0});
        mesh.positions.push_back({1, 0, 0});
        std::vector<uint32_t> indices = {0, 1};
        polygon::triangulate(mesh, indices);
        REQUIRE(mesh.triangles.empty());
    }
}

TEST_CASE("triangulate handles a simple triangle") {
    SimpleMesh3d mesh;
    mesh.positions.push_back({0, 0, 0});
    mesh.positions.push_back({1, 0, 0});
    mesh.positions.push_back({0, 1, 0});

    std::vector<uint32_t> indices = {0, 1, 2};
    polygon::triangulate(mesh, indices);

    REQUIRE(mesh.triangles.size() == 1);
    CHECK(mesh::compare_equality_triangles(mesh.triangles[0], {0, 1, 2}));
}

void check_every_vertex_used_at_least_once(const SimpleMesh3d &mesh) {
    std::vector<uint32_t> counts(mesh.positions.size(), 0);
    for (const auto &triangle : mesh.triangles) {
        for (size_t i = 0; i < 3; i++) {
            const uint32_t vertex_index = triangle[i];
            counts[vertex_index]++;
        }
    }

    for (size_t i = 0; i < mesh.positions.size(); i++) {
        CHECK(counts[i] >= 1);
    }
}

TEST_CASE("triangulate handles a square") {
    SimpleMesh3d mesh;
    mesh.positions.push_back({0, 0, 0});
    mesh.positions.push_back({1, 0, 0});
    mesh.positions.push_back({1, 1, 0});
    mesh.positions.push_back({0, 1, 0});

    std::vector<uint32_t> indices = {0, 1, 2, 3};
    polygon::triangulate(mesh, indices);

    REQUIRE(mesh.triangles.size() == 2); // A square should produce 2 triangles
    check_every_vertex_used_at_least_once(mesh);
    mesh::validate(mesh);
}

TEST_CASE("triangulate works with UVs") {
    SimpleMesh3d mesh;
    mesh.positions.push_back({0, 0, 0});
    mesh.positions.push_back({1, 0, 0});
    mesh.positions.push_back({1, 1, 0});
    mesh.positions.push_back({0, 1, 0});
    mesh.uvs.resize(4);

    std::vector<uint32_t> indices = {0, 1, 2, 3};
    polygon::triangulate(mesh, indices);

    REQUIRE(mesh.uvs.size() == mesh.positions.size());
}

std::optional<glm::dvec2> intersection(
    const glm::uvec2 &e1,
    const glm::uvec2 &e2,
    const std::span<glm::dvec2> &positions) {
    const glm::dvec2 &p = positions[e1.x];
    const glm::dvec2 r = positions[e1.y] - positions[e1.x];

    const glm::dvec2 &q = positions[e2.x];
    const glm::dvec2 s = positions[e2.y] - positions[e2.x];

    double rxs = r.x * s.y - r.y * s.x; // 2D cross product
    glm::dvec2 qmp = q - p;
    double qpxr = qmp.x * r.y - qmp.y * r.x;

    if (std::abs(rxs) < 1e-6) {
        return std::nullopt; // Lines are parallel
    }

    double t = (qmp.x * s.y - qmp.y * s.x) / rxs;
    double u = qpxr / rxs;

    if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
        return p + t * r; // intersection point
    }

    return std::nullopt; // No intersection
}

TEST_CASE("triangulate handles U-shaped polygon") {
    SimpleMesh3d mesh;

    // Define U-shape polygon
    mesh.positions.push_back({0, 0, 0}); // 0
    mesh.positions.push_back({3, 0, 0}); // 1
    mesh.positions.push_back({3, 1, 0}); // 2
    mesh.positions.push_back({2, 1, 0}); // 3
    mesh.positions.push_back({2, 2, 0}); // 4
    mesh.positions.push_back({1, 2, 0}); // 5
    mesh.positions.push_back({1, 1, 0}); // 6
    mesh.positions.push_back({0, 1, 0}); // 7

    std::vector<uint32_t> indices = {0, 1, 2, 3, 4, 5, 6, 7};

    polygon::triangulate(mesh, indices);
    REQUIRE(!mesh.triangles.empty());
    check_every_vertex_used_at_least_once(mesh);
    mesh::validate(mesh);

    // Check that all boundary edges are present in the triangulation
    const std::vector<glm::uvec2> boundary_edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 0}
    };
    std::vector<glm::uvec2> edges;
    for (const auto &triangle : mesh.triangles) {
        for (size_t i = 0; i < 3; i++) {
            const glm::uvec2 edge = {triangle[i], triangle[(i + 1) % 3]};
            edges.push_back(edge);
        }
    }
    for (const auto &boundary_edge : boundary_edges) {
        bool found = false;
        for (const auto &edge : edges) {
            if (boundary_edge == edge) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    // Check that no edge crosses the polygon boundary
    std::vector<glm::dvec2> positions_2d;
    for (const auto &position : mesh.positions) {
        positions_2d.push_back({position.x, position.y});
    }

    for (const auto &edge : edges) {
        for (const auto& boundary_edge : boundary_edges) {
            const auto intersection_opt = intersection(edge, boundary_edge, positions_2d);
            if (intersection_opt.has_value()) {
                const glm::dvec2 intersection_point = intersection_opt.value();
                // Check if the intersection point is not an endpoint of either edge
                if (intersection_point != positions_2d[edge.x] &&
                    intersection_point != positions_2d[edge.y] &&
                    intersection_point != positions_2d[boundary_edge.x] &&
                    intersection_point != positions_2d[boundary_edge.y]) {
                    FAIL("Edge crosses polygon boundary");
                }
            }
        }
    }
}
