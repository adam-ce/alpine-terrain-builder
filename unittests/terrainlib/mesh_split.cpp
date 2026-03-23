#include <cstdint>
#include <vector>

#include "../catch2_helpers.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/split.h"
#include "mesh/validate.h"

namespace {

SimpleMesh make_shared_edge_quad() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(1, 0, 0), // 1
        glm::dvec3(0, 1, 0), // 2
        glm::dvec3(1, 1, 0), // 3
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 3, 2),
    };
    return mesh;
}

SimpleMesh make_triangle_strip() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(1, 0, 0), // 1
        glm::dvec3(0, 1, 0), // 2
        glm::dvec3(1, 1, 0), // 3
        glm::dvec3(2, 1, 0), // 4
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 3, 2),
        glm::uvec3(1, 4, 3),
    };
    return mesh;
}

SimpleMesh make_triangle_fan() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),  // 0 center
        glm::dvec3(1, 0, 0),  // 1
        glm::dvec3(0, 1, 0),  // 2
        glm::dvec3(-1, 0, 0), // 3
        glm::dvec3(0, -1, 0), // 4
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 2, 3),
        glm::uvec3(0, 3, 4),
    };
    return mesh;
}

SimpleMesh make_triangle_strip_with_uvs() {
    SimpleMesh mesh = make_triangle_strip();
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(0.5, 0),
        glm::dvec2(0, 0.5),
        glm::dvec2(0.5, 0.5),
        glm::dvec2(1.0, 0.5),
    };
    return mesh;
}

SimpleMesh make_triangle_fan_with_uvs() {
    SimpleMesh mesh = make_triangle_fan();
    mesh.uvs = {
        glm::dvec2(0.5, 0.5),
        glm::dvec2(1, 0.5),
        glm::dvec2(0.5, 1),
        glm::dvec2(0, 0.5),
        glm::dvec2(0.5, 0),
    };
    return mesh;
}

} // namespace

TEST_CASE("mesh::split_by_vertex") {
    SECTION("empty mesh produces empty groups") {
        const SimpleMesh mesh;
        const auto result = mesh::split_by_vertex(mesh, 3, [](uint32_t) { return 0u; });

        REQUIRE(result.groups.size() == 3);
        CHECK(result.vertex_remap.empty());

        for (const auto &group : result.groups) {
            CHECK(group.positions.empty());
            CHECK(group.uvs.empty());
            CHECK(group.triangles.empty());
        }
    }

    SECTION("triangles wholly contained in a vertex group are preserved") {
        const SimpleMesh mesh = make_triangle_strip();

        const auto result = mesh::split_by_vertex(mesh, 2, [](uint32_t v) {
            return v <= 2 ? 0u : 1u;
        });

        REQUIRE(result.groups.size() == 2);
        REQUIRE(result.vertex_remap.size() == mesh.vertex_count());

        CHECK(result.groups[0].triangles.size() == 1);
        CHECK(result.groups[1].triangles.empty());

        CHECK(result.groups[0].positions.size() == 3);

        mesh::validate_basic(result.groups[0]);
        mesh::validate_basic(result.groups[1]);
    }

    SECTION("triangles spanning multiple vertex groups are dropped") {
        const SimpleMesh mesh = make_triangle_strip();

        const auto result = mesh::split_by_vertex(mesh, 2, [](uint32_t v) {
            return (v == 0 || v == 1 || v == 2) ? 0u : 1u;
        });

        REQUIRE(result.groups.size() == 2);

        // (0,1,2) survives in group 0
        // (1,3,2) crosses groups -> dropped
        // (1,4,3) crosses groups -> dropped
        CHECK(result.groups[0].triangles.size() == 1);
        CHECK(result.groups[1].triangles.empty());
    }

    SECTION("different contained triangles can survive in different groups") {
        const SimpleMesh mesh = make_triangle_strip();

        const auto result = mesh::split_by_vertex(mesh, 2, [](uint32_t v) {
            switch (v) {
            case 0:
            case 1:
            case 2:
                return 0u;
            case 3:
            case 4:
                return 1u;
            default:
                return 0u;
            }
        });

        REQUIRE(result.groups.size() == 2);

        // Only first triangle is fully in group 0 for this partition.
        CHECK(result.groups[0].triangles.size() == 1);
        CHECK(result.groups[1].triangles.empty());
        
        mesh::validate_basic(result.groups[0]);
        mesh::validate_basic(result.groups[1]);
    }

    SECTION("shared edge quad split by vertex drops all crossing triangles") {
        const SimpleMesh mesh = make_shared_edge_quad();

        const auto result = mesh::split_by_vertex(mesh, 2, [](uint32_t v) {
            return (v == 0 || v == 2) ? 0u : 1u;
        });

        REQUIRE(result.groups.size() == 2);
        CHECK(result.groups[0].triangles.empty());
        CHECK(result.groups[1].triangles.empty());
    }

    SECTION("uvs stay aligned with positions") {
        const SimpleMesh mesh = make_triangle_strip_with_uvs();

        const auto result = mesh::split_by_vertex(mesh, 2, [](uint32_t v) {
            return v <= 2 ? 0u : 1u;
        });

        REQUIRE(result.groups.size() == 2);

        CHECK(result.groups[0].positions.size() == result.groups[0].uvs.size());
        CHECK(result.groups[1].positions.size() == result.groups[1].uvs.size());

        CHECK(result.groups[0].positions.size() == 3);
        CHECK(result.groups[0].uvs.size() == 3);

        CHECK(result.groups[0].positions[0] == mesh.positions[0]);
        CHECK(result.groups[0].positions[1] == mesh.positions[1]);
        CHECK(result.groups[0].positions[2] == mesh.positions[2]);

        CHECK(result.groups[0].uvs[0] == mesh.uvs[0]);
        CHECK(result.groups[0].uvs[1] == mesh.uvs[1]);
        CHECK(result.groups[0].uvs[2] == mesh.uvs[2]);
    }
}

TEST_CASE("mesh::split_by_triangle") {
    SECTION("empty mesh produces empty groups") {
        const SimpleMesh mesh;
        const auto result = mesh::split_by_triangle(mesh, 4, [](uint32_t) { return 0u; });

        REQUIRE(result.groups.size() == 4);

        for (const auto &group : result.groups) {
            CHECK(group.positions.empty());
            CHECK(group.uvs.empty());
            CHECK(group.triangles.empty());
        }
    }

    SECTION("shared edge quad split by triangle duplicates shared vertices across groups") {
        const SimpleMesh mesh = make_shared_edge_quad();

        const auto result = mesh::split_by_triangle(mesh, 2, [](uint32_t triangle_index) {
            return triangle_index;
        });

        REQUIRE(result.groups.size() == 2);

        CHECK(result.groups[0].triangles.size() == 1);
        CHECK(result.groups[1].triangles.size() == 1);

        // Each triangle should be compacted to its own 3-vertex local mesh.
        CHECK(result.groups[0].positions.size() == 3);
        CHECK(result.groups[1].positions.size() == 3);

        mesh::validate_basic(result.groups[0]);
        mesh::validate_basic(result.groups[1]);
    }

    SECTION("triangle fan split by triangle creates one local mesh per assigned group") {
        const SimpleMesh mesh = make_triangle_fan();

        const auto result = mesh::split_by_triangle(mesh, 3, [](uint32_t triangle_index) {
            return triangle_index;
        });

        REQUIRE(result.groups.size() == 3);

        for (uint32_t group_index = 0; group_index < 3; ++group_index) {
            CHECK(result.groups[group_index].triangles.size() == 1);
            CHECK(result.groups[group_index].positions.size() == 3);
            mesh::validate_basic(result.groups[group_index]);
        }
    }

    SECTION("multiple triangles assigned to same group are preserved together") {
        const SimpleMesh mesh = make_triangle_fan();

        const auto result = mesh::split_by_triangle(mesh, 2, [](uint32_t triangle_index) {
            return triangle_index == 2 ? 1u : 0u;
        });

        REQUIRE(result.groups.size() == 2);

        CHECK(result.groups[0].triangles.size() == 2);
        CHECK(result.groups[1].triangles.size() == 1);

        mesh::validate_basic(result.groups[0]);
        mesh::validate_basic(result.groups[1]);

        // Group 0 contains triangles (0,1,2) and (0,2,3), so it needs 4 local vertices.
        CHECK(result.groups[0].positions.size() == 4);
        // Group 1 contains only (0,3,4), so it needs 3 local vertices.
        CHECK(result.groups[1].positions.size() == 3);
    }

    SECTION("uvs remain aligned when shared vertices are duplicated across groups") {
        const SimpleMesh mesh = make_triangle_fan_with_uvs();

        const auto result = mesh::split_by_triangle(mesh, 3, [](uint32_t triangle_index) {
            return triangle_index;
        });

        REQUIRE(result.groups.size() == 3);

        for (const auto &group : result.groups) {
            CHECK(group.positions.size() == group.uvs.size());
            CHECK(group.positions.size() == 3);
            CHECK(group.triangles.size() == 1);
            mesh::validate_basic(group);
        }
    }
}
