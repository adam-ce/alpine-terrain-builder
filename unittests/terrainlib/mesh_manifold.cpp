#include <algorithm>
#include <array>
#include <vector>

#include <fmt/core.h>

#include "../catch2_helpers.h"
#include "mesh/SimpleMesh.h"
#include "mesh/manifold.h"

namespace {

void sort_edges(std::span<glm::uvec2> edges) {
    for (auto &edge : edges) {
        mesh::normalize_edge_inplace(edge);
    }

    std::sort(edges.begin(), edges.end(), [](const glm::uvec2 &a, const glm::uvec2 &b) {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.y < b.y;
    });
}

SimpleMesh make_quad() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
        glm::dvec3(1, 1, 0)};
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 3, 2)};
    return mesh;
}

// Three triangles sharing the same edge (0, 1) -> edge non-manifold.
SimpleMesh make_edge_non_manifold_mesh() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),  // 0
        glm::dvec3(1, 0, 0),  // 1
        glm::dvec3(0, 1, 0),  // 2
        glm::dvec3(0, -1, 0), // 3
        glm::dvec3(0.5, 0, 1) // 4
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(1, 0, 3),
        glm::uvec3(0, 1, 4)};
    return mesh;
}

// Two fans touching only at vertex 0 -> vertex non-manifold, but edges manifold.
SimpleMesh make_vertex_non_manifold_mesh() {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),  // 0 shared articulation
        glm::dvec3(1, 0, 0),  // 1
        glm::dvec3(0, 1, 0),  // 2
        glm::dvec3(-1, 0, 0), // 3
        glm::dvec3(0, -1, 0), // 4
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 3, 4)};
    return mesh;
}

} // namespace

TEST_CASE("mesh::find_non_manifold_edges") {
    SECTION("manifold mesh has no non-manifold edges") {
        const SimpleMesh mesh = make_quad();

        const auto actual = mesh::find_non_manifold_edges(mesh.triangles);

        CHECK(actual.empty());
    }

    SECTION("detects one non-manifold edge from raw triangles") {
        const SimpleMesh mesh = make_edge_non_manifold_mesh();

        auto actual = mesh::find_non_manifold_edges(mesh.triangles);
        sort_edges(actual);

        std::vector<glm::uvec2> expected = {glm::uvec2(0, 1)};
        sort_edges(expected);

        CHECK(actual == expected);
    }

    SECTION("detects one non-manifold edge from SimpleMesh overload") {
        const SimpleMesh mesh = make_edge_non_manifold_mesh();

        auto actual = mesh::find_non_manifold_edges(mesh);
        sort_edges(actual);

        std::vector<glm::uvec2> expected = {glm::uvec2(0, 1)};
        sort_edges(expected);

        CHECK(actual == expected);
    }

    SECTION("duplicate edge orientation does not matter") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 0, 3),
            glm::uvec3(1, 0, 4)};

        auto actual = mesh::find_non_manifold_edges(std::span<const glm::uvec3>(triangles));
        sort_edges(actual);

        std::vector<glm::uvec2> expected = {glm::uvec2(0, 1)};
        sort_edges(expected);

        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::is_edge_manifold") {
    SECTION("quad is edge manifold") {
        const SimpleMesh mesh = make_quad();

        CHECK(mesh::is_edge_manifold(mesh));
        CHECK(mesh::is_edge_manifold(mesh.triangles));
    }

    SECTION("three triangles sharing same edge is not edge manifold") {
        const SimpleMesh mesh = make_edge_non_manifold_mesh();

        CHECK_FALSE(mesh::is_edge_manifold(mesh));
        CHECK_FALSE(mesh::is_edge_manifold(mesh.triangles));
    }

    SECTION("vertex-only non-manifold mesh is still edge manifold") {
        const SimpleMesh mesh = make_vertex_non_manifold_mesh();

        CHECK(mesh::is_edge_manifold(mesh));
        CHECK(mesh::is_edge_manifold(mesh.triangles));
    }
}

TEST_CASE("mesh::is_vertex_manifold") {
    SECTION("quad is vertex manifold") {
        const SimpleMesh mesh = make_quad();

        CHECK(mesh::is_vertex_manifold(mesh));
        CHECK(mesh::is_vertex_manifold(mesh.triangles));
        CHECK(mesh::is_vertex_manifold(mesh.triangles, static_cast<uint32_t>(mesh.positions.size())));
    }

    SECTION("two disconnected fans sharing one vertex is not vertex manifold") {
        const SimpleMesh mesh = make_vertex_non_manifold_mesh();

        CHECK_FALSE(mesh::is_vertex_manifold(mesh));
        CHECK_FALSE(mesh::is_vertex_manifold(mesh.triangles));
        CHECK_FALSE(mesh::is_vertex_manifold(mesh.triangles, static_cast<uint32_t>(mesh.positions.size())));
    }

    SECTION("edge non-manifold mesh is also not manifold as a whole") {
        const SimpleMesh mesh = make_edge_non_manifold_mesh();

        CHECK_FALSE(mesh::is_vertex_manifold(mesh));
    }
}

TEST_CASE("mesh::is_manifold") {
    SECTION("quad is manifold") {
        const SimpleMesh mesh = make_quad();

        CHECK(mesh::is_manifold(mesh));
        CHECK(mesh::is_manifold(mesh.triangles));
        CHECK(mesh::is_manifold(mesh.triangles, static_cast<uint32_t>(mesh.positions.size())));
    }

    SECTION("edge non-manifold mesh is not manifold") {
        const SimpleMesh mesh = make_edge_non_manifold_mesh();

        CHECK_FALSE(mesh::is_manifold(mesh));
        CHECK_FALSE(mesh::is_manifold(mesh.triangles));
        CHECK_FALSE(mesh::is_manifold(mesh.triangles, static_cast<uint32_t>(mesh.positions.size())));
    }

    SECTION("vertex non-manifold mesh is not manifold") {
        const SimpleMesh mesh = make_vertex_non_manifold_mesh();

        CHECK_FALSE(mesh::is_manifold(mesh));
        CHECK_FALSE(mesh::is_manifold(mesh.triangles));
        CHECK_FALSE(mesh::is_manifold(mesh.triangles, static_cast<uint32_t>(mesh.positions.size())));
    }

    SECTION("empty mesh is manifold") {
        const SimpleMesh mesh;

        CHECK(mesh::is_manifold(mesh));
        CHECK(mesh::is_edge_manifold(mesh.triangles));
        CHECK(mesh::is_vertex_manifold(mesh.triangles, 0));
    }
}

TEST_CASE("mesh::duplicate_non_manifold_edges") {
    SECTION("manifold mesh stays unchanged") {
        SimpleMesh actual = make_quad();
        const SimpleMesh expected = actual;

        mesh::duplicate_non_manifold_edges(actual);

        CHECK(actual.positions == expected.positions);
        CHECK(actual.uvs == expected.uvs);
        CHECK(actual.triangles == expected.triangles);
    }

    SECTION("repairs edge non-manifold mesh") {
        SimpleMesh actual = make_edge_non_manifold_mesh();

        REQUIRE_FALSE(mesh::is_edge_manifold(actual));
        const auto old_position_count = actual.positions.size();
        const auto old_triangle_count = actual.triangles.size();

        mesh::duplicate_non_manifold_edges(actual);

        CHECK(mesh::is_edge_manifold(actual));
        CHECK(actual.triangles.size() == old_triangle_count);
        CHECK(actual.positions.size() > old_position_count);
    }

    SECTION("positions and uvs are duplicated together") {
        SimpleMesh actual = make_edge_non_manifold_mesh();
        actual.uvs = {
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
            glm::dvec2(1, 1),
            glm::dvec2(0.5, 0.5)};

        REQUIRE(actual.positions.size() == actual.uvs.size());

        mesh::duplicate_non_manifold_edges(actual.triangles, actual.positions, actual.uvs);

        CHECK(mesh::is_edge_manifold(actual.triangles));
        CHECK(actual.positions.size() == actual.uvs.size());
        CHECK(actual.positions.size() > 5);
    }

    SECTION("custom duplicate callback is invoked") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 0, 3),
            glm::uvec3(0, 1, 4)};

        uint32_t vertex_count = 5;
        uint32_t duplicate_calls = 0;

        mesh::duplicate_non_manifold_edges(triangles, [&](uint32_t original_vertex) -> uint32_t {
            CAPTURE(original_vertex);
            ++duplicate_calls;
            return vertex_count++;
        });

        CHECK(duplicate_calls >= 1);
        CHECK(mesh::is_edge_manifold(triangles));
    }
}

TEST_CASE("mesh::duplicate_non_manifold_vertices") {
    SECTION("manifold mesh stays unchanged") {
        SimpleMesh actual = make_quad();
        const SimpleMesh expected = actual;

        mesh::duplicate_non_manifold_vertices(actual);

        CHECK(actual.positions == expected.positions);
        CHECK(actual.uvs == expected.uvs);
        CHECK(actual.triangles == expected.triangles);
    }

    SECTION("repairs vertex non-manifold mesh") {
        SimpleMesh actual = make_vertex_non_manifold_mesh();

        REQUIRE_FALSE(mesh::is_vertex_manifold(actual));
        const auto old_position_count = actual.positions.size();
        const auto old_triangle_count = actual.triangles.size();

        mesh::duplicate_non_manifold_vertices(actual);

        CHECK(mesh::is_vertex_manifold(actual));
        CHECK(actual.triangles.size() == old_triangle_count);
        CHECK(actual.positions.size() > old_position_count);
    }

    SECTION("positions and uvs remain aligned") {
        SimpleMesh actual = make_vertex_non_manifold_mesh();
        actual.uvs = {
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
            glm::dvec2(-1, 0),
            glm::dvec2(0, -1)};

        REQUIRE(actual.positions.size() == actual.uvs.size());

        mesh::duplicate_non_manifold_vertices(actual.triangles, actual.positions, actual.uvs);

        CHECK(mesh::is_vertex_manifold(actual.triangles, static_cast<uint32_t>(actual.positions.size())));
        CHECK(actual.positions.size() == actual.uvs.size());
        CHECK(actual.positions.size() > 5);
    }

    SECTION("custom duplicate callback is invoked") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 3, 4)};

        uint32_t vertex_count = 5;
        uint32_t duplicate_calls = 0;

        mesh::duplicate_non_manifold_vertices(triangles, vertex_count, [&](uint32_t original_vertex) -> uint32_t {
            CAPTURE(original_vertex);
            ++duplicate_calls;
            return vertex_count++;
        });

        CHECK(duplicate_calls >= 1);
        CHECK(mesh::is_vertex_manifold(triangles, vertex_count));
    }

    SECTION("fixes multiple disconnected fans in one pass") {
        SimpleMesh mesh;
        mesh.positions = {
            glm::dvec3(0, 0, 0),   // 0
            glm::dvec3(1, 0, 0),   // 1
            glm::dvec3(0, 1, 0),   // 2
            glm::dvec3(-1, 0, 0),  // 3
            glm::dvec3(0, -1, 0),  // 4
            glm::dvec3(2, 0, 0),   // 5
            glm::dvec3(2, 1, 0),   // 6
            glm::dvec3(-2, 0, 0),  // 7
            glm::dvec3(-2, -1, 0), // 8
            glm::dvec3(3, 0, 0),   // 9
            glm::dvec3(3, 1, 0),   // 10
            glm::dvec3(-3, 0, 0),  // 11
            glm::dvec3(-3, -1, 0)  // 12
        };

        // Vertex 0 is a bow-tie:
        //   fan A: (0,1,2), (0,2,5)
        //   fan B: (0,3,4), (0,4,7)
        //
        // Vertex 5 is also non-manifold:
        //   fan A: (0,2,5), (5,2,6)
        //   fan B: (5,9,10)
        //
        // Vertex 7 is also non-manifold:
        //   fan A: (0,4,7), (7,4,8)
        //   fan B: (7,11,12)
        mesh.triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 2, 5),

            glm::uvec3(0, 3, 4),
            glm::uvec3(0, 4, 7),

            glm::uvec3(5, 2, 6),
            glm::uvec3(5, 9, 10),

            glm::uvec3(7, 4, 8),
            glm::uvec3(7, 11, 12),
        };

        CHECK_FALSE(mesh::is_vertex_manifold(mesh));

        mesh::duplicate_non_manifold_vertices(mesh);

        CHECK(mesh::is_vertex_manifold(mesh));
    }
}

TEST_CASE("mesh::make_manifold") {
    SECTION("manifold mesh stays manifold") {
        SimpleMesh actual = make_quad();
        const auto old_positions = actual.positions;
        const auto old_triangles = actual.triangles;

        mesh::make_manifold(actual);

        CHECK(mesh::is_manifold(actual));
        CHECK(actual.positions == old_positions);
        CHECK(actual.triangles == old_triangles);
    }

    SECTION("repairs edge non-manifold mesh") {
        SimpleMesh actual = make_edge_non_manifold_mesh();

        REQUIRE_FALSE(mesh::is_manifold(actual));

        mesh::make_manifold(actual);

        CHECK(mesh::is_manifold(actual));
        CHECK(mesh::is_edge_manifold(actual));
    }

    SECTION("repairs vertex non-manifold mesh") {
        SimpleMesh actual = make_vertex_non_manifold_mesh();

        REQUIRE_FALSE(mesh::is_manifold(actual));

        mesh::make_manifold(actual);

        CHECK(mesh::is_manifold(actual));
        CHECK(mesh::is_vertex_manifold(actual));
    }

    SECTION("repairs both edge and vertex issues through vector overload") {
        std::vector<glm::dvec3> positions = {
            glm::dvec3(0, 0, 0),   // 0
            glm::dvec3(1, 0, 0),   // 1
            glm::dvec3(0, 1, 0),   // 2
            glm::dvec3(0, -1, 0),  // 3
            glm::dvec3(0.5, 0, 1), // 4
            glm::dvec3(-1, 0, 0),  // 5
            glm::dvec3(0, -2, 0)   // 6
        };

        // First three create a non-manifold edge (0,1), last adds disconnected fan at vertex 0.
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 0, 3),
            glm::uvec3(0, 1, 4),
            glm::uvec3(0, 5, 6)};

        REQUIRE_FALSE(mesh::is_manifold(triangles, static_cast<uint32_t>(positions.size())));

        mesh::make_manifold(triangles, positions);

        CHECK(mesh::is_manifold(triangles, static_cast<uint32_t>(positions.size())));
    }

    SECTION("uv overload keeps positions and uvs aligned") {
        SimpleMesh actual = make_vertex_non_manifold_mesh();
        actual.uvs = {
            glm::dvec2(0, 0),
            glm::dvec2(1, 0),
            glm::dvec2(0, 1),
            glm::dvec2(-1, 0),
            glm::dvec2(0, -1)};

        REQUIRE(actual.positions.size() == actual.uvs.size());

        mesh::make_manifold(actual.triangles, actual.positions, actual.uvs);

        CHECK(mesh::is_manifold(actual.triangles, static_cast<uint32_t>(actual.positions.size())));
        CHECK(actual.positions.size() == actual.uvs.size());
    }

    SECTION("custom duplicate callback can make mesh manifold") {
        std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 0, 3),
            glm::uvec3(0, 1, 4),
            glm::uvec3(0, 5, 6)};

        uint32_t vertex_count = 7;
        uint32_t duplicate_calls = 0;

        mesh::make_manifold(triangles, vertex_count, [&](uint32_t original_vertex) -> uint32_t {
            CAPTURE(original_vertex);
            ++duplicate_calls;
            return vertex_count++;
        });

        CHECK(duplicate_calls >= 1);
        CHECK(mesh::is_manifold(triangles, vertex_count));
    }
}
