#include <algorithm>
#include <cstdint>
#include <vector>

#include "../catch2_helpers.h"
#include "mesh/SimpleMesh.h"
#include "mesh/cleanup.h"

namespace {
std::vector<uint32_t> sorted(std::vector<uint32_t> v) {
    std::sort(v.begin(), v.end());
    return v;
}

SimpleMesh make_unique_triangle_mesh() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(1, 0, 0), // 1
        glm::dvec3(0, 1, 0), // 2
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
    };
    return mesh;
}

SimpleMesh make_same_indices_duplicate_mesh() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(1, 0, 0), // 1
        glm::dvec3(0, 1, 0), // 2
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 1, 2),
    };
    return mesh;
}

SimpleMesh make_orientation_flipped_duplicate_mesh() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0
        glm::dvec3(1, 0, 0), // 1
        glm::dvec3(0, 1, 0), // 2
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 2, 1),
    };
    return mesh;
}

SimpleMesh make_mesh_with_isolated_vertices() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0), // 0 used
        glm::dvec3(1, 0, 0), // 1 used
        glm::dvec3(0, 1, 0), // 2 used
        glm::dvec3(5, 5, 5), // 3 isolated
        glm::dvec3(6, 6, 6), // 4 isolated
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
        glm::dvec2(5, 5),
        glm::dvec2(6, 6),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
    };
    return mesh;
}

SimpleMesh make_mesh_with_no_isolated_vertices() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
        glm::dvec3(1, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 3, 2),
    };
    return mesh;
}

SimpleMesh make_mesh_with_degenerate_triangles() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
        glm::dvec3(1, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2), // valid
        glm::uvec3(0, 0, 1), // degenerate
        glm::uvec3(2, 3, 3), // degenerate
        glm::uvec3(1, 3, 2), // valid
    };
    return mesh;
}

SimpleMesh make_mesh_with_one_tiny_and_one_large_triangle() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),  // 0
        glm::dvec3(10.0, 0.0, 0.0), // 1
        glm::dvec3(0.0, 10.0, 0.0), // 2  large triangle

        glm::dvec3(0.0, 0.0, 1.0),   // 3
        glm::dvec3(0.001, 0.0, 1.0), // 4
        glm::dvec3(0.0, 0.001, 1.0), // 5  tiny triangle
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(3, 4, 5),
    };
    return mesh;
}

} // namespace

TEST_CASE("mesh::find_duplicate_triangles") {

    SECTION("unique mesh has no duplicates") {
        const SimpleMesh mesh = make_unique_triangle_mesh();

        CHECK(mesh::find_duplicate_triangles(mesh, false).empty());
        CHECK(mesh::find_duplicate_triangles(mesh, true).empty());
        CHECK(mesh::find_duplicate_triangles(mesh.triangles, mesh.positions, false).empty());
        CHECK(mesh::find_duplicate_triangles(mesh.triangles, mesh.positions, true).empty());
        CHECK(mesh::find_duplicate_triangles_consider_orientation(mesh.triangles).empty());
        CHECK(mesh::find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions).empty());
    }

    SECTION("exact duplicate indices are found regardless of orientation mode") {
        const SimpleMesh mesh = make_same_indices_duplicate_mesh();

        const auto a = sorted(mesh::find_duplicate_triangles(mesh, false));
        const auto b = sorted(mesh::find_duplicate_triangles(mesh, true));
        const auto c = sorted(mesh::find_duplicate_triangles(mesh.triangles, mesh.positions, false));
        const auto d = sorted(mesh::find_duplicate_triangles(mesh.triangles, mesh.positions, true));
        const auto e = sorted(mesh::find_duplicate_triangles_consider_orientation(mesh.triangles));
        const auto f = sorted(mesh::find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions));

        REQUIRE(a.size() == 1);
        REQUIRE(b.size() == 1);
        REQUIRE(c.size() == 1);
        REQUIRE(d.size() == 1);
        REQUIRE(e.size() == 1);
        REQUIRE(f.size() == 1);

        CHECK((a[0] == 0 || a[0] == 1));
        CHECK((b[0] == 0 || b[0] == 1));
        CHECK((c[0] == 0 || c[0] == 1));
        CHECK((d[0] == 0 || d[0] == 1));
        CHECK((e[0] == 0 || e[0] == 1));
        CHECK((f[0] == 0 || f[0] == 1));
    }

    SECTION("orientation-flipped triangle is duplicate only when orientation is ignored") {
        const SimpleMesh mesh = make_orientation_flipped_duplicate_mesh();

        CHECK(mesh::find_duplicate_triangles(mesh, false).empty());
        CHECK(mesh::find_duplicate_triangles(mesh.triangles, mesh.positions, false).empty());
        CHECK(mesh::find_duplicate_triangles_consider_orientation(mesh.triangles).empty());

        const auto a = sorted(mesh::find_duplicate_triangles(mesh, true));
        const auto b = sorted(mesh::find_duplicate_triangles(mesh.triangles, mesh.positions, true));
        const auto c = sorted(mesh::find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions));

        REQUIRE(a.size() == 1);
        REQUIRE(b.size() == 1);
        REQUIRE(c.size() == 1);

        CHECK((a[0] == 0 || a[0] == 1));
        CHECK((b[0] == 0 || b[0] == 1));
        CHECK((c[0] == 0 || c[0] == 1));
    }
}

TEST_CASE("mesh::remove_duplicate_triangles") {
    SECTION("removing from unique mesh changes nothing") {
        SimpleMesh mesh = make_unique_triangle_mesh();
        const auto old_triangles = mesh.triangles;

        mesh::remove_duplicate_triangles(mesh, false);
        CHECK(mesh.triangles == old_triangles);

        mesh::remove_duplicate_triangles(mesh, true);
        CHECK(mesh.triangles == old_triangles);
    }

    SECTION("removes exact duplicate triangles") {
        SimpleMesh mesh = make_same_indices_duplicate_mesh();

        mesh::remove_duplicate_triangles(mesh, false);

        REQUIRE(mesh.triangles.size() == 1);
        CHECK(mesh.triangles[0] == glm::uvec3(0, 1, 2));
    }

    SECTION("removing with ignore orientation removes flipped duplicate") {
        SimpleMesh mesh = make_orientation_flipped_duplicate_mesh();

        mesh::remove_duplicate_triangles(mesh, true);

        REQUIRE(mesh.triangles.size() == 1);
        CHECK(mesh::compare_equality_triangles_ignore_orientation(
            mesh.triangles[0], glm::uvec3(0, 1, 2)));
    }

    SECTION("consider orientation keeps flipped duplicate") {
        SimpleMesh mesh = make_orientation_flipped_duplicate_mesh();

        mesh::remove_duplicate_triangles_consider_orientation(mesh);

        REQUIRE(mesh.triangles.size() == 2);
        CHECK(mesh.triangles[0] == glm::uvec3(0, 1, 2));
        CHECK(mesh.triangles[1] == glm::uvec3(0, 2, 1));
    }

    SECTION("consider orientation triangles-only overload removes exact duplicates") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 1),
        };

        mesh::remove_duplicate_triangles_consider_orientation(triangles);

        REQUIRE(triangles.size() == 2);
        CHECK(triangles[0] == glm::uvec3(0, 1, 2));
        CHECK(triangles[1] == glm::uvec3(0, 2, 1));
    }
}

TEST_CASE("mesh::remove_isolated_vertices") {
    SECTION("mesh without isolated vertices stays unchanged") {
        SimpleMesh mesh = make_mesh_with_no_isolated_vertices();
        const auto old_positions = mesh.positions;
        const auto old_triangles = mesh.triangles;

        const size_t removed = mesh::remove_isolated_vertices(mesh);

        CHECK(removed == 0);
        CHECK(mesh.positions == old_positions);
        CHECK(mesh.triangles == old_triangles);
    }

    SECTION("isolated vertices are removed and triangles remain valid") {
        SimpleMesh mesh = make_mesh_with_isolated_vertices();

        const size_t removed = mesh::remove_isolated_vertices(mesh);

        CHECK(removed == 2);
        REQUIRE(mesh.positions.size() == 3);
        REQUIRE(mesh.uvs.size() == 3);
        REQUIRE(mesh.triangles.size() == 1);

        CHECK(mesh.triangles[0] == glm::uvec3(0, 1, 2));
        CHECK(mesh.positions[0] == glm::dvec3(0, 0, 0));
        CHECK(mesh.positions[1] == glm::dvec3(1, 0, 0));
        CHECK(mesh.positions[2] == glm::dvec3(0, 1, 0));
    }

    SECTION("all vertices can become isolated after triangle removal by caller") {
        SimpleMesh mesh = make_unique_triangle_mesh();
        mesh.triangles.clear();

        const size_t removed = mesh::remove_isolated_vertices(mesh);

        CHECK(removed == 3);
        CHECK(mesh.positions.empty());
        CHECK(mesh.triangles.empty());
    }
}

TEST_CASE("mesh::remove_degenerate_triangles") {
    SECTION("removes triangles with repeated vertex indices") {
        SimpleMesh mesh = make_mesh_with_degenerate_triangles();

        mesh::remove_degenerate_triangles(mesh.triangles);

        REQUIRE(mesh.triangles.size() == 2);
        CHECK(mesh.triangles[0] == glm::uvec3(0, 1, 2));
        CHECK(mesh.triangles[1] == glm::uvec3(1, 3, 2));
    }

    SECTION("mesh with no degenerate triangles stays unchanged") {
        SimpleMesh mesh = make_mesh_with_no_isolated_vertices();
        const auto old_triangles = mesh.triangles;

        mesh::remove_degenerate_triangles(mesh.triangles);

        CHECK(mesh.triangles == old_triangles);
    }
}

TEST_CASE("mesh::remove_triangles_of_negligible_size") {
    SECTION("removes very small triangles relative to average size") {
        SimpleMesh mesh = make_mesh_with_one_tiny_and_one_large_triangle();

        const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 1.0);

        CHECK(removed == 1);
        REQUIRE(mesh.triangles.size() == 1);
        CHECK(mesh.triangles[0] == glm::uvec3(0, 1, 2));
    }

    SECTION("threshold zero removes nothing") {
        SimpleMesh mesh = make_mesh_with_one_tiny_and_one_large_triangle();
        const auto old_triangles = mesh.triangles;

        const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 0.0);

        CHECK(removed == 0);
        CHECK(mesh.triangles == old_triangles);
    }

    SECTION("mesh with similarly sized triangles remains unchanged for small threshold") {
        SimpleMesh mesh = make_mesh_with_no_isolated_vertices();
        const auto old_triangles = mesh.triangles;

        const size_t removed = mesh::remove_triangles_of_negligible_size(mesh, 0.01);

        CHECK(removed == 0);
        CHECK(mesh.triangles == old_triangles);
    }
}
