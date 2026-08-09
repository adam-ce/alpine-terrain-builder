#include <algorithm>
#include <array>
#include <unordered_set>
#include <vector>

#include <fmt/core.h>

#include "../catch2_helpers.h"
#include "mesh/topology/adjacency.h"
#include "mesh/topology/topology.h"
#include "mesh/topology/edges.h"
#include "mesh/topology/triangle_compare.h"
#include "mesh/topology/vertex_index_range.h"
#include "mesh/normalize.h"

namespace {

void sort_edges(std::vector<glm::uvec2> &edges) {
    std::sort(edges.begin(), edges.end(), [](const glm::uvec2 &a, const glm::uvec2 &b) {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.y < b.y;
    });
}
std::vector<glm::uvec2> sorted_edges_from_set(const std::unordered_set<glm::uvec2> &set) {
    std::vector<glm::uvec2> result(set.begin(), set.end());
    sort_edges(result);
    return result;
}

} // namespace

TEST_CASE("mesh::is_degenerate") {
    SECTION("triangle with three distinct vertices is not degenerate") {
        CHECK_FALSE(mesh::is_degenerate(glm::uvec3(0, 1, 2)));
    }

    SECTION("triangle with repeated first and second vertex is degenerate") {
        CHECK(mesh::is_degenerate(glm::uvec3(1, 1, 2)));
    }

    SECTION("triangle with repeated first and third vertex is degenerate") {
        CHECK(mesh::is_degenerate(glm::uvec3(3, 4, 3)));
    }

    SECTION("triangle with repeated second and third vertex is degenerate") {
        CHECK(mesh::is_degenerate(glm::uvec3(5, 6, 6)));
    }
}

TEST_CASE("mesh::sort_and_normalize_triangles(span)") {
    SECTION("normalizes each triangle and sorts whole triangle list") {
        std::vector<glm::uvec3> actual = {
            glm::uvec3(4, 2, 3),
            glm::uvec3(2, 0, 1),
            glm::uvec3(3, 1, 2)};

        mesh::sort_and_normalize_triangles(actual);

        std::vector<glm::uvec3> expected = {
            mesh::normalize_triangle(glm::uvec3(2, 0, 1)),
            mesh::normalize_triangle(glm::uvec3(3, 1, 2)),
            mesh::normalize_triangle(glm::uvec3(4, 2, 3))};

        CHECK(actual == expected);
    }

    SECTION("empty input stays empty") {
        std::vector<glm::uvec3> triangles;
        mesh::sort_and_normalize_triangles(triangles);
        CHECK(triangles.empty());
    }
}

TEST_CASE("mesh::flip_triangle_orientation and flip_triangle_orientations") {
    SECTION("flip_triangle_orientation swaps winding") {
        glm::uvec3 actual(1, 2, 3);

        mesh::flip_triangle_orientation(actual);

        CHECK(mesh::normalize_triangle(actual) == glm::uvec3(1, 3, 2));
    }

    SECTION("flip_triangle_orientations flips all triangles in span") {
        std::vector<glm::uvec3> actual = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(3, 4, 5)};

        mesh::flip_triangle_orientations(actual);
        mesh::normalize_triangles_inplace(actual);

        std::vector<glm::uvec3> expected = {
            glm::uvec3(0, 2, 1),
            glm::uvec3(3, 5, 4)};

        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::compute_vertex_count") {
    SECTION("returns zero for empty triangle list") {
        const std::vector<glm::uvec3> triangles;
        CHECK(mesh::compute_vertex_count(triangles) == 0);
    }

    SECTION("works without isolated vertices") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 2, 1),
            glm::uvec3(4, 3, 2),
            glm::uvec3(5, 1, 6)};

        CHECK(mesh::compute_vertex_count(triangles) == 7);
    }

    SECTION("works with isolated vertices") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 2, 1),
            glm::uvec3(7, 1, 6)};

        CHECK(mesh::compute_vertex_count(triangles) == 5);
    }
}

TEST_CASE("mesh::get_edges last overload") {
    SECTION("returns all directed edges when normalize is false") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3)};

        auto actual = mesh::get_halfedges(triangles);
        sort_edges(actual);

        const std::vector<glm::uvec2> expected = {
            glm::uvec2(0, 1),
            glm::uvec2(1, 2),
            glm::uvec2(1, 3),
            glm::uvec2(2, 0),
            glm::uvec2(2, 1),
            glm::uvec2(3, 2)};

        CHECK(actual == expected);
    }

    SECTION("shared reversed edge collapses when normalize is true") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3)};

        const auto actual = sorted_edges_from_set(mesh::get_edges(triangles));

        const std::vector<glm::uvec2> expected = {
            glm::uvec2(0, 1),
            glm::uvec2(0, 2),
            glm::uvec2(1, 2),
            glm::uvec2(1, 3),
            glm::uvec2(2, 3)};

        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::for_each_halfedge") {
    SECTION("iterates all directed edges in triangle order") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3)};

        std::vector<glm::uvec2> actual;
        mesh::for_each_halfedge(triangles, [&](const glm::uvec2 &edge) { actual.push_back(edge); }, false);

        const std::vector<glm::uvec2> expected = {
            glm::uvec2(0, 1),
            glm::uvec2(1, 2),
            glm::uvec2(2, 0),
            glm::uvec2(2, 1),
            glm::uvec2(1, 3),
            glm::uvec2(3, 2)};

        CHECK(actual == expected);
    }

    SECTION("iterates normalized edges when requested") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(2, 0, 1)};

        std::vector<glm::uvec2> actual;
        mesh::for_each_halfedge(triangles, [&](const glm::uvec2 &edge) { actual.push_back(edge); }, true);
        sort_edges(actual);

        const std::vector<glm::uvec2> expected = {
            glm::uvec2(0, 1),
            glm::uvec2(0, 2),
            glm::uvec2(1, 2)};

        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::other_vertices_in_triangle") {
    SECTION("returns the two non-matching vertices") {
        CHECK(mesh::other_vertices_in_triangle(glm::uvec3(5, 7, 9), 5) == glm::uvec2(7, 9));
        CHECK(mesh::other_vertices_in_triangle(glm::uvec3(5, 7, 9), 7) == glm::uvec2(5, 9));
        CHECK(mesh::other_vertices_in_triangle(glm::uvec3(5, 7, 9), 9) == glm::uvec2(5, 7));
    }
}

TEST_CASE("mesh::change_vertex and change_vertex_inplace") {
    SECTION("change_vertex returns modified copy") {
        const glm::uvec3 actual = mesh::change_vertex(glm::uvec3(1, 2, 3), 2, 9);
        CHECK(actual == glm::uvec3(1, 9, 3));
    }

    SECTION("change_vertex_inplace modifies matching entry") {
        glm::uvec3 actual(1, 2, 3);

        mesh::change_vertex_inplace(actual, 3, 8);

        CHECK(actual == glm::uvec3(1, 2, 8));
    }

    SECTION("non-existing old vertex leaves triangle unchanged") {
        glm::uvec3 actual(1, 2, 3);

        mesh::change_vertex_inplace(actual, 99, 8, true);

        CHECK(actual == glm::uvec3(1, 2, 3));
    }
}

TEST_CASE("mesh::normalize_face_index_rotation") {
    SECTION("rotates smallest vertex to the front while keeping orientation") {
        std::array<uint32_t, 4> actual = {4, 7, 1, 3};

        mesh::normalize_face_index_rotation(std::span<uint32_t>(actual), true);

        CHECK(actual == std::array<uint32_t, 4>{1, 3, 4, 7});
    }

    SECTION("also normalizes when keep_orientation is false") {
        std::array<uint32_t, 4> actual = {4, 7, 1, 3};

        mesh::normalize_face_index_rotation(std::span<uint32_t>(actual), false);

        CHECK(*std::min_element(actual.begin(), actual.end()) == actual.front());
    }
}

TEST_CASE("mesh::normalize_edge and normalize_edge_inplace") {
    SECTION("returns ascending edge") {
        CHECK(mesh::normalize_edge(glm::uvec2(7, 2)) == glm::uvec2(2, 7));
        CHECK(mesh::normalize_edge(glm::uvec2(2, 7)) == glm::uvec2(2, 7));
    }

    SECTION("inplace variant sorts ascending") {
        glm::uvec2 actual(9, 3);
        mesh::normalize_edge_inplace(actual);
        CHECK(actual == glm::uvec2(3, 9));
    }
}

TEST_CASE("mesh::normalize_triangle and normalize_triangle_inplace") {
    SECTION("keep_orientation keeps winding while rotating smallest index first") {
        const glm::uvec3 actual = mesh::normalize_triangle(glm::uvec3(5, 2, 4), true);
        CHECK(actual == glm::uvec3(2, 4, 5));
    }

    SECTION("inplace variant matches value-returning variant") {
        glm::uvec3 actual(8, 3, 6);
        const glm::uvec3 expected = mesh::normalize_triangle(actual, true);

        mesh::normalize_triangle_inplace(actual, true);

        CHECK(actual == expected);
    }

    SECTION("ignore-orientation result compares equal ignoring orientation") {
        const glm::uvec3 source(5, 2, 4);
        const glm::uvec3 actual = mesh::normalize_triangle(source, false);

        CHECK(mesh::compare_equality_triangles_ignore_orientation(source, actual));
    }
}

TEST_CASE("mesh::normalize_quad and normalize_quad_inplace") {
    SECTION("rotates smallest index first with orientation kept") {
        const glm::uvec4 actual = mesh::normalize_quad(glm::uvec4(4, 7, 1, 3), true);
        CHECK(actual == glm::uvec4(1, 3, 4, 7));
    }

    SECTION("inplace variant matches value-returning variant") {
        glm::uvec4 actual(9, 5, 2, 6);
        const glm::uvec4 expected = mesh::normalize_quad(actual, true);

        mesh::normalize_quad_inplace(actual, true);

        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::compare triangle helpers") {
    SECTION("compare_triangles uses normalized ordering") {
        CHECK(mesh::compare_triangles(glm::uvec3(0, 1, 2), glm::uvec3(1, 2, 3)));
        CHECK_FALSE(mesh::compare_triangles(glm::uvec3(4, 1, 2), glm::uvec3(1, 2, 3)));
    }

    SECTION("ignore_orientation treats reversed winding as same triangle") {
        CHECK(mesh::compare_triangles_ignore_orientation(glm::uvec3(0, 1, 2), glm::uvec3(1, 0, 2)));
    }

    SECTION("strict equality distinguishes orientation") {
        CHECK(mesh::compare_equality_triangles(glm::uvec3(0, 1, 2), glm::uvec3(0, 1, 2)));
        CHECK_FALSE(mesh::compare_equality_triangles(glm::uvec3(0, 1, 2), glm::uvec3(0, 2, 1)));
    }

    SECTION("ignore_orientation equality accepts cyclic or reversed versions") {
        CHECK(mesh::compare_equality_triangles_ignore_orientation(glm::uvec3(0, 1, 2), glm::uvec3(1, 2, 0)));
        CHECK(mesh::compare_equality_triangles_ignore_orientation(glm::uvec3(0, 1, 2), glm::uvec3(0, 2, 1)));
    }
}

TEST_CASE("mesh::create_edge_to_triangle_mapping_manifold") {
    SECTION("maps each normalized edge to one or two incident triangles") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3)};

        const auto actual = mesh::create_edge_to_triangle_mapping_manifold(triangles);

        REQUIRE(actual.contains(glm::uvec2(0, 1)));
        REQUIRE(actual.contains(glm::uvec2(0, 2)));
        REQUIRE(actual.contains(glm::uvec2(1, 2)));
        REQUIRE(actual.contains(glm::uvec2(1, 3)));
        REQUIRE(actual.contains(glm::uvec2(2, 3)));

        CHECK(actual.at(glm::uvec2(0, 1)).size() == 1);
        CHECK(actual.at(glm::uvec2(0, 2)).size() == 1);
        CHECK(actual.at(glm::uvec2(1, 2)).size() == 2);
        CHECK(actual.at(glm::uvec2(1, 3)).size() == 1);
        CHECK(actual.at(glm::uvec2(2, 3)).size() == 1);
    }
}

TEST_CASE("mesh::create_edge_to_triangle_mapping_non_manifold") {
    SECTION("stores all incident triangles for non-manifold edge") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 0, 3),
            glm::uvec3(0, 1, 4)};

        const auto actual = mesh::create_edge_to_triangle_mapping_non_manifold(triangles);

        REQUIRE(actual.contains(glm::uvec2(0, 1)));
        CHECK(actual.at(glm::uvec2(0, 1)).size() == 3);
    }
}

TEST_CASE("mesh::create_edge_to_triangle_mapping_non_manifold2") {
    SECTION("stores all incident triangles in std::vector") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(1, 0, 3),
            glm::uvec3(0, 1, 4)};

        const auto actual = mesh::create_edge_to_triangle_mapping_non_manifold2(triangles);

        REQUIRE(actual.contains(glm::uvec2(0, 1)));
        CHECK(actual.at(glm::uvec2(0, 1)) == std::vector<uint32_t>{0, 1, 2});
    }
}

TEST_CASE("mesh::create_vertex_to_triangle_mapping") {
    SECTION("maps each vertex to all incident triangles") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3)};

        const auto actual = mesh::create_vertex_to_triangle_mapping(triangles);

        REQUIRE(actual.size() == 4);
        CHECK(actual[0] == std::vector<uint32_t>{0});
        CHECK(actual[1] == std::vector<uint32_t>{0, 1});
        CHECK(actual[2] == std::vector<uint32_t>{0, 1});
        CHECK(actual[3] == std::vector<uint32_t>{1});
    }
}

TEST_CASE("mesh::count_vertex_adjacent_triangles") {
    SECTION("counts incident triangles per vertex") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(1, 2, 3),
            glm::uvec3(3, 2, 4)};

        const auto actual = mesh::count_vertex_adjacent_triangles(triangles);

        CHECK(actual == std::vector<uint32_t>{0, 1, 2, 2, 1});
    }
}

TEST_CASE("mesh::is_consistently_oriented") {
    SECTION("empty mesh is orientable") {
        const std::vector<glm::uvec3> triangles = {};
        CHECK(mesh::is_consistently_oriented(triangles));
    }

    SECTION("single triangle is orientable") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2)};
        CHECK(mesh::is_consistently_oriented(triangles));
    }

    SECTION("two properly oriented adjacent triangles are orientable") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3) // opposite orientation along shared edge (1,2)
        };
        CHECK(mesh::is_consistently_oriented(triangles));
    }

    SECTION("two triangles with same directed edge are not orientable") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 1, 3) // same directed edge (0 -> 1)
        };

        CHECK_FALSE(mesh::is_consistently_oriented(triangles));
    }

    SECTION("identical triangles are not orientable") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(0, 1, 2)};

        CHECK_FALSE(mesh::is_consistently_oriented(triangles));
    }

    SECTION("edge shared by more than two triangles") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3),
            glm::uvec3(4, 1, 2) // third triangle sharing edge (1,2)
        };

        CHECK_FALSE(mesh::is_consistently_oriented(triangles));
    }
}

TEST_CASE("mesh::find_isolated_vertices") {
    SECTION("returns vertices not referenced by any triangle") {
        const mesh::Simple mesh(
            {
                glm::uvec3(0, 1, 2),
                glm::uvec3(2, 1, 3)
            }, {
                glm::dvec3(0),
                glm::dvec3(1),
                glm::dvec3(2),
                glm::dvec3(3),
                glm::dvec3(4),
                glm::dvec3(5),
            }
        );

        const auto actual = mesh::find_isolated_vertices(mesh);

        CHECK(actual == std::vector<uint32_t>{4, 5});
    }

    SECTION("returns empty when all vertices are used") {
        const std::vector<glm::uvec3> triangles = {
            glm::uvec3(0, 1, 2),
            glm::uvec3(2, 1, 3)};

        const auto actual = mesh::find_isolated_vertices(triangles);

        CHECK(actual.empty());
    }
}

TEST_CASE("mesh::compute_topology empty mesh") {
    const std::vector<glm::uvec3> triangles;

    const auto t = mesh::compute_topology(triangles);

    CHECK(t.component_count() == 0);
    CHECK(t.boundary_count() == 0);
    CHECK(t.chi() == 0);
    CHECK(t.genus() == 0);

    CHECK(t.is_empty());
    CHECK_FALSE(t.is_open());
    CHECK_FALSE(t.is_closed());
    CHECK_FALSE(t.is_disk(false));
    CHECK_FALSE(t.is_disk(true));
    CHECK_FALSE(t.is_annulus());
    CHECK_FALSE(t.is_sphere());
    CHECK_FALSE(t.is_torus());
}

TEST_CASE("mesh::compute_topology single triangle") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 2)};

    const auto t = mesh::compute_topology(triangles);

    REQUIRE_FALSE(t.is_empty());
    REQUIRE(t.is_single_component());

    const auto &c = t.component(0);

    CHECK(c.vertex_count() == 3);
    CHECK(c.edge_count() == 3);
    CHECK(c.halfedge_count() == 3);
    CHECK(c.triangle_count() == 1);
    CHECK(c.boundary_count() == 1);
    CHECK(c.boundary_edge_count() == 3);
    CHECK(c.chi() == 1);
    CHECK(c.genus() == 0);

    CHECK(t.component_count() == 1);
    CHECK(t.boundary_count() == 1);
    CHECK(t.chi() == 1);
    CHECK(t.genus() == 0);

    CHECK_FALSE(t.is_empty());
    CHECK(t.is_open());
    CHECK_FALSE(t.is_closed());
    CHECK(t.is_disk(false));
    CHECK(t.is_disk(true));
    CHECK_FALSE(t.is_annulus());
    CHECK_FALSE(t.is_sphere());
    CHECK_FALSE(t.is_torus());
}

TEST_CASE("mesh::compute_topology square patch") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(2, 1, 3)};

    const auto t = mesh::compute_topology(triangles);

    REQUIRE_FALSE(t.is_empty());
    REQUIRE(t.is_single_component());

    const auto &c = t.component(0);

    CHECK(c.vertex_count() == 4);
    CHECK(c.edge_count() == 5);
    CHECK(c.halfedge_count() == 6);
    CHECK(c.triangle_count() == 2);
    CHECK(c.boundary_count() == 1);
    CHECK(c.boundary_edge_count() == 4);
    CHECK(c.chi() == 1);
    CHECK(c.genus() == 0);

    CHECK(t.component_count() == 1);
    CHECK(t.boundary_count() == 1);
    CHECK(t.chi() == 1);
    CHECK(t.genus() == 0);

    CHECK_FALSE(t.is_empty());
    CHECK(t.is_open());
    CHECK_FALSE(t.is_closed());
    CHECK(t.is_disk(false));
    CHECK(t.is_disk(true));
    CHECK_FALSE(t.is_annulus());
    CHECK_FALSE(t.is_sphere());
    CHECK_FALSE(t.is_torus());
}

TEST_CASE("mesh::compute_topology annulus") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 5),
        glm::uvec3(0, 5, 4),
        glm::uvec3(1, 2, 6),
        glm::uvec3(1, 6, 5),
        glm::uvec3(2, 3, 7),
        glm::uvec3(2, 7, 6),
        glm::uvec3(3, 0, 4),
        glm::uvec3(3, 4, 7)};

    const auto t = mesh::compute_topology(triangles);

    REQUIRE_FALSE(t.is_empty());
    REQUIRE(t.is_single_component());

    const auto &c = t.component(0);

    CHECK(c.vertex_count() == 8);
    CHECK(c.edge_count() == 16);
    CHECK(c.triangle_count() == 8);
    CHECK(c.boundary_count() == 2);
    CHECK(c.boundary_edge_count() == 8);
    CHECK(c.chi() == 0);
    CHECK(c.genus() == 0);

    CHECK(t.component_count() == 1);
    CHECK(t.boundary_count() == 2);
    CHECK(t.chi() == 0);
    CHECK(t.genus() == 0);

    CHECK_FALSE(t.is_empty());
    CHECK(t.is_open());
    CHECK_FALSE(t.is_closed());
    CHECK_FALSE(t.is_disk(false));
    CHECK(t.is_disk(true));
    CHECK(t.is_annulus());
    CHECK_FALSE(t.is_sphere());
    CHECK_FALSE(t.is_torus());
}

TEST_CASE("mesh::compute_topology tetrahedron sphere") {
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 2, 1),
        glm::uvec3(0, 3, 2),
        glm::uvec3(1, 2, 3),
        glm::uvec3(0, 1, 3)};

    const auto t = mesh::compute_topology(triangles);

    REQUIRE_FALSE(t.is_empty());
    REQUIRE(t.is_single_component());

    const auto &c = t.component(0);

    CHECK(c.vertex_count() == 4);
    CHECK(c.edge_count() == 6);
    CHECK(c.triangle_count() == 4);
    CHECK(c.boundary_count() == 0);
    CHECK(c.boundary_edge_count() == 0);
    CHECK(c.chi() == 2);
    CHECK(c.genus() == 0);

    CHECK(t.component_count() == 1);
    CHECK(t.boundary_count() == 0);
    CHECK(t.chi() == 2);
    CHECK(t.genus() == 0);

    CHECK_FALSE(t.is_empty());
    CHECK_FALSE(t.is_open());
    CHECK(t.is_closed());
    CHECK_FALSE(t.is_disk(false));
    CHECK_FALSE(t.is_disk(true));
    CHECK_FALSE(t.is_annulus());
    CHECK(t.is_sphere());
    CHECK_FALSE(t.is_torus());
}

TEST_CASE("mesh::compute_topology torus") {
    // 3x3 periodic torus grid:
    // V = 9, F = 18, E = 27, chi = 0, genus = 1
    const std::vector<glm::uvec3> triangles = {
        glm::uvec3(0, 1, 4), glm::uvec3(0, 4, 3),
        glm::uvec3(1, 2, 5), glm::uvec3(1, 5, 4),
        glm::uvec3(2, 0, 3), glm::uvec3(2, 3, 5),

        glm::uvec3(3, 4, 7), glm::uvec3(3, 7, 6),
        glm::uvec3(4, 5, 8), glm::uvec3(4, 8, 7),
        glm::uvec3(5, 3, 6), glm::uvec3(5, 6, 8),

        glm::uvec3(6, 7, 1), glm::uvec3(6, 1, 0),
        glm::uvec3(7, 8, 2), glm::uvec3(7, 2, 1),
        glm::uvec3(8, 6, 0), glm::uvec3(8, 0, 2)};

    const auto t = mesh::compute_topology(triangles);

    REQUIRE_FALSE(t.is_empty());
    REQUIRE(t.is_single_component());

    const auto &c = t.component(0);

    CHECK(c.vertex_count() == 9);
    CHECK(c.edge_count() == 27);
    CHECK(c.triangle_count() == 18);
    CHECK(c.boundary_count() == 0);
    CHECK(c.boundary_edge_count() == 0);
    CHECK(c.chi() == 0);
    CHECK(c.genus() == 1);

    CHECK(t.component_count() == 1);
    CHECK(t.boundary_count() == 0);
    CHECK(t.chi() == 0);
    CHECK(t.genus() == 1);

    CHECK_FALSE(t.is_empty());
    CHECK_FALSE(t.is_open());
    CHECK(t.is_closed());
    CHECK_FALSE(t.is_disk(false));
    CHECK_FALSE(t.is_disk(true));
    CHECK_FALSE(t.is_annulus());
    CHECK_FALSE(t.is_sphere());
    CHECK(t.is_torus());
}
