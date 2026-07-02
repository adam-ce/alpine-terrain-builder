#include <algorithm>
#include <vector>

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

        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("all edges for single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        auto edges = mesh::find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected = {
            {0, 1}, {1, 2}, {2, 0}};
        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {1, 1, 0}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        auto edges = mesh::find_boundary_edges(mesh);

        std::vector<glm::uvec2> actual(edges.begin(), edges.end());
        std::vector<glm::uvec2> expected = {
            {0, 1}, {2, 0}, {1, 3}, {3, 2} // shared edge {1,2} not included
        };

        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
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

        CHECK_THAT(actual, Catch::Matchers::UnorderedEquals(expected));
    }
}

void normalize_boundary(std::vector<uint32_t> &boundary) {
    if (boundary.empty()) {
        return;
    }
    auto min_it = std::min_element(boundary.begin(), boundary.end());
    std::rotate(boundary.begin(), min_it, boundary.end());
}

void normalize_boundaries(std::vector<std::vector<uint32_t>> &boundaries) {
    for (auto &b : boundaries) {
        normalize_boundary(b);
    }
    std::sort(boundaries.begin(), boundaries.end());
}

TEST_CASE("mesh::find_boundaries") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }

    SECTION("single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 2}};
        CHECK(actual == expected);
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {1, 1, 0}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 3, 2}};
        CHECK(actual == expected);
    }

    SECTION("two separate triangles") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {0, 1, 0}, // 2
            {2, 0, 0}, // 3
            {3, 0, 0}, // 4
            {2, 1, 0}  // 5
        };
        mesh.triangles = {
            {0, 1, 2},
            {3, 4, 5}};

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {3, 4, 5}
        };
        CHECK(actual == expected);
    }

    SECTION("square with triangular hole") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {1, 1, 0}, // 2
            {0, 1, 0}, // 3
            {0.25f, 0.25f, 0}, // 4
            {0.75f, 0.25f, 0}, // 5
            {0.5f, 0.75f, 0}   // 6
        };
        mesh.triangles = {
            {0, 1, 4}, {1, 5, 4}, {1, 2, 5}, {2, 6, 5}, {2, 3, 6}, {3, 4, 6}, {3, 0, 4}
        };

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2, 3},
            {4, 6, 5},
        };
        CHECK(actual == expected);
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

        auto actual = mesh::find_boundaries(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::find_boundaries_non_manifold") {
    SECTION("empty for empty mesh") {
        SimpleMesh mesh;
        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }

    SECTION("single triangle") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0}};
        mesh.triangles = {glm::uvec3(0, 1, 2)};

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 2}};
        CHECK(actual == expected);
    }

    SECTION("two triangles sharing an edge") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {1, 1, 0}};
        mesh.triangles = {
            {0, 1, 2},
            {1, 3, 2}};

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {{0, 1, 3, 2}};
        CHECK(actual == expected);
    }

    SECTION("two separate triangles") {
        SimpleMesh mesh;
        mesh.positions = {
            {0, 0, 0}, // 0
            {1, 0, 0}, // 1
            {0, 1, 0}, // 2
            {2, 0, 0}, // 3
            {3, 0, 0}, // 4
            {2, 1, 0}  // 5
        };
        mesh.triangles = {
            {0, 1, 2},
            {3, 4, 5}};

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {3, 4, 5}};
        CHECK(actual == expected);
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
            {1, 5, 6},
            {1, 6, 2}, // right
            {5, 4, 7},
            {5, 7, 6}, // back
            {4, 0, 3},
            {4, 3, 7}, // left
            {3, 2, 6},
            {3, 6, 7}, // top
            {4, 5, 1},
            {4, 1, 0} // bottom
        };

        auto actual = mesh::find_boundaries_non_manifold(mesh);
        normalize_boundaries(actual);
        const std::vector<std::vector<uint32_t>> expected = {};
        CHECK(actual == expected);
    }

    SECTION("bowtie sharing vertex") {
        std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {1, 3, 4},
        };

        auto actual = mesh::find_boundaries_non_manifold(triangles);
        normalize_boundaries(actual);

        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {1, 3, 4},
        };

        CHECK(actual == expected);
    }

    SECTION("three triangles sharing vertex") {
        std::vector<glm::uvec3> triangles = {
            {0, 1, 2},
            {0, 3, 4},
            {0, 5, 6}
        };

        auto actual = mesh::find_boundaries_non_manifold(triangles);
        normalize_boundaries(actual);

        const std::vector<std::vector<uint32_t>> expected = {
            {0, 1, 2},
            {0, 3, 4},
            {0, 5, 6}
        };

        CHECK(actual == expected);
    }
}

TEST_CASE("mesh::build_boundary_triangle_mask") {
    SECTION("open box with more triangles than vertices") {
        // 5 faces of a cube (top removed): 8 vertices, 10 triangles
        std::vector<glm::uvec3> triangles = {
            {0, 1, 2}, {0, 2, 3},   // front
            {1, 5, 6}, {1, 6, 2},   // right
            {5, 4, 7}, {5, 7, 6},   // back
            {4, 0, 3}, {4, 3, 7},   // left
            {4, 5, 1}, {4, 1, 0},   // bottom
        };
        REQUIRE(triangles.size() > 8);

        std::vector<uint8_t> mask;
        mesh::build_boundary_triangle_mask<uint8_t>(triangles, mask, 1, 0);

        const std::vector<uint8_t> expected = {0, 1, 0, 1, 0, 1, 0, 1, 0, 0};
        CHECK(mask == expected);
    }
}
