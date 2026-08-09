#include <glm/glm.hpp>

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/TriangleSoup.h"
#include "mesh/cgal.h"
#include "mesh/clip.h"
#include "mesh/convert.h"
#include "mesh/io.h"
#include "mesh/bounds.h"
#include "mesh/validate.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/geometry.h"
#include "octree/Space.h"
#include "octree/Id.h"

TEST_CASE("empty mesh") {
    const SimpleMesh mesh;
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1.0, -1.0, -1.0), glm::dvec3(1.0, 1.0, 1.0));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    CHECK(clipped_mesh.positions.empty());
    CHECK(clipped_mesh.triangles.empty());
    CHECK(clipped_mesh.uvs.empty());
    CHECK(!clipped_mesh.texture.has_value());
}

TEST_CASE("single triangle in bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(1, 1, 1));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    CHECK(mesh.positions == clipped_mesh.positions);
    CHECK(mesh.uvs == clipped_mesh.uvs);
    CHECK(mesh.triangles == clipped_mesh.triangles);
}

TEST_CASE("single triangle out of bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(-0.1, -0.1, -0.1));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    CHECK(clipped_mesh.positions == std::vector<glm::dvec3>{});
    CHECK(clipped_mesh.uvs == std::vector<glm::dvec2>{});
    CHECK(clipped_mesh.triangles == std::vector<glm::uvec3>{});
}

TEST_CASE("single triangle touching bounds in point") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.uvs = {
        glm::dvec2(0, 0),
        glm::dvec2(1, 0),
        glm::dvec2(0, 1),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0, 0, 0));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    CHECK(clipped_mesh.positions == std::vector<glm::dvec3>{});
    CHECK(clipped_mesh.uvs == std::vector<glm::dvec2>{});
    CHECK(clipped_mesh.triangles == std::vector<glm::uvec3>{});
}

TEST_CASE("single triangle touching bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.5, 0.5, 0));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);
    CHECK(clipped_soup.size() == 2);
    CHECK(clipped_soup == TriangleSoup{{
                                        glm::dvec3(0, 0, 0),
                                        glm::dvec3(0.5, 0, 0),
                                        glm::dvec3(0.5, 0.5, 0),
                                    },
                                    {
                                        glm::dvec3(0, 0, 0),
                                        glm::dvec3(0.5, 0.5, 0),
                                        glm::dvec3(0, 0.5, 0),
                                    }});
}

TEST_CASE("single triangle with single vertex in bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0, 0, 0),
        glm::dvec3(1, 0, 0),
        glm::dvec3(0, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.5, 0.5, 0.5));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);
    if (clipped_soup.size() == 1) {
        CHECK(clipped_soup == TriangleSoup{{glm::dvec3(0, 0, 0),
                                            glm::dvec3(0.5, 0, 0),
                                            glm::dvec3(0, 0.5, 0)}});
    } else {
        CHECK(clipped_soup == TriangleSoup{{
                                               glm::dvec3(0, 0, 0),
                                               glm::dvec3(0.5, 0, 0),
                                               glm::dvec3(0.5, 0.5, 0),
                                           },
                                           {
                                               glm::dvec3(0, 0, 0),
                                               glm::dvec3(0.5, 0.5, 0),
                                               glm::dvec3(0, 0.5, 0),
                                           }});
    }
}

TEST_CASE("clip returns empty mesh if triangle with vertices on bounds and outside") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),  // on plane y = 0
        glm::dvec3(1.0, 0.0, 0.0),  // on plane y = 0
        glm::dvec3(0.5, -1.0, 0.0), // outside (below plane)
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};

    // Clip with bottom plane y >= 0
    const radix::geometry::Aabb3d bounds(
        glm::dvec3(-1.0, 0.0, -1.0),
        glm::dvec3(2.0, 1.0, 1.0));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    CAPTURE(clipped_soup);
    CHECK(clipped_soup == TriangleSoup{});
}

TEST_CASE("single triangle around bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 3.0, 0.0),
        glm::dvec3(3.0, -2.0, 0.0),
        glm::dvec3(-3.0, -2.0, 0.0),
    };
    mesh.triangles = {glm::uvec3(0, 1, 2)};

    const radix::geometry::Aabb3d bounds(
        glm::dvec3(-1.0),
        glm::dvec3(1.0));
    const radix::geometry::Aabb2d bounds2d(
        glm::dvec2(bounds.min),
        glm::dvec2(bounds.max));

    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    double total_area = 0.0;
    for (const auto &triangle : clipped_soup) {
        const auto &a = triangle[0];
        const auto &b = triangle[1];
        const auto &c = triangle[2];

        CAPTURE(a, b, c);

        // Still on the source plane.
        CHECK(a.z == Catch::Approx(0.0));
        CHECK(b.z == Catch::Approx(0.0));
        CHECK(c.z == Catch::Approx(0.0));

        // Every emitted vertex must lie inside the clipping bounds.
        CHECK(bounds2d.contains_inclusive(glm::dvec2(a)));
        CHECK(bounds2d.contains_inclusive(glm::dvec2(b)));
        CHECK(bounds2d.contains_inclusive(glm::dvec2(c)));

        total_area += geometry::compute_triangle_area(a, b, c);
    }

    // The clipped region is exactly a 2x2 square.
    CHECK(total_area == Catch::Approx(4.0));
}

TEST_CASE("single triangle with two vertices in bounds") {
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(-0.5, 0, 0),
        glm::dvec3(0.5, 0, 0),
        glm::dvec3(0.5, 1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(-1, -1, -1), glm::dvec3(0.5, 0.5, 0.5));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    const TriangleSoup expected1 = {
        {glm::dvec3(-0.5, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)},
        {glm::dvec3(-0.5, 0, 0), glm::dvec3(0.5, 0.5, 0), glm::dvec3(0.0, 0.5, 0)}};

    const TriangleSoup expected2 = {
        {glm::dvec3(-0.5, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.0, 0.5, 0)},
        {glm::dvec3(0.0, 0.5, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)}};

    CAPTURE(clipped_soup);
    CAPTURE(expected1);
    CAPTURE(expected2);
    CHECK((clipped_soup == expected1 || clipped_soup == expected2));
}

TEST_CASE("single triangle split by one plane and partly discarded by the next") {
    // The x plane leaves two vertices inside and splits the triangle in two. The piece clipped
    // first falls entirely outside the y plane, while the one held back still reaches into bounds.
    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(-1, 1, 0),
        glm::dvec3(1, -3, 0),
        glm::dvec3(9, -1, 0),
    };
    mesh.triangles = {
        glm::uvec3(0, 1, 2)};
    const radix::geometry::Aabb3d bounds(glm::dvec3(0, 0, -1), glm::dvec3(10, 10, 1));
    const SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
    const TriangleSoup clipped_soup = to_sorted_triangle_soup(clipped_mesh);

    double total_area = 0.0;
    for (const auto &triangle : clipped_soup) {
        const auto &a = triangle[0];
        const auto &b = triangle[1];
        const auto &c = triangle[2];

        CAPTURE(a, b, c);
        CHECK(bounds.contains_inclusive(a));
        CHECK(bounds.contains_inclusive(b));
        CHECK(bounds.contains_inclusive(c));

        total_area += geometry::compute_triangle_area(a, b, c);
    }

    // The clipped region is the triangle (0, 0), (4, 0), (0, 0.8).
    CAPTURE(clipped_soup);
    CHECK(!clipped_soup.empty());
    CHECK(total_area == Catch::Approx(1.6));
}

void run_checks(const SimpleMesh &mesh, const SimpleMesh &clipped_mesh, const radix::geometry::Aabb3d &bounds) {
    mesh::validate(clipped_mesh);

    // Check that no vertices are outside the bounds
    for (const auto &position : clipped_mesh.positions) {
        // INFO(fmt::format("Vertex: {}", position));
        CHECK(bounds.contains_inclusive(position));
    }

    // Check that the source triangles that were fully inside the bounds are still present
    // and the ones that are outside removed or changed.
    for (const auto &triangle : mesh.triangles) {
        CAPTURE(triangle);
        CAPTURE(mesh.positions[triangle[0]]);
        CAPTURE(mesh.positions[triangle[1]]);
        CAPTURE(mesh.positions[triangle[2]]);
        bool should_be_kept = true;
        for (uint32_t i = 0; i < 3; ++i) {
            if (!bounds.contains_inclusive(mesh.positions[triangle[i]])) {
                should_be_kept = false;
                break;
            }
        }

        bool found = false;
        std::array<glm::dvec3, 3> vertices = {
            mesh.positions[triangle.x],
            mesh.positions[triangle.y],
            mesh.positions[triangle.z]};
        for (const auto &clipped_triangle : clipped_mesh.triangles) {
            bool matches = true;
            for (uint32_t i = 0; i < 3; ++i) {
                const auto &clipped_position = clipped_mesh.positions[clipped_triangle[i]];
                if (std::find(vertices.begin(), vertices.end(), clipped_position) == vertices.end()) {
                    matches = false;
                    CAPTURE(clipped_triangle);
                    break;
                }
            }
            if (matches) {
                found = true;
                break;
            }
        }

        CHECK(should_be_kept == found);
    }
}

TEST_CASE("clip_on_bounds produces manifold mesh") {
    SimpleMesh mesh;
    mesh.positions = {
        // Triangle A
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(-1.0, 0.0, 0.0),
        glm::dvec3(-1.0, 1.0, 0.0),

        // Triangle B (disconnected, but will intersect at same point)
        glm::dvec3(1.0, 0.0, 0.2),
        glm::dvec3(-1.0, 0.0, -0.2),
        glm::dvec3(-1.0, -1.0, -0.2),
    };

    mesh.triangles = {
        glm::uvec3(0, 1, 2),
        glm::uvec3(3, 4, 5),
    };

    const radix::geometry::Aabb3d bounds(
        glm::dvec3(0.0, -10.0, -10.0),
        glm::dvec3(10.0, 10.0, 10.0));

    const SimpleMesh clipped = mesh::clip_on_bounds(mesh, bounds);

    CHECK(clipped.triangles.size() == 2);
    CHECK(mesh::is_manifold(clipped));
    CHECK(clipped.positions.size() == 6);
}

#ifdef NDEBUG
TEST_CASE("mesh::clip_on_bounds benchmark") {
    BENCHMARK_ADVANCED("clip based on octree")(Catch::Benchmark::Chronometer meter) {
        const std::filesystem::path mesh_path = ATB_TEST_DATA_DIR "/meshes/6857.terrain";
        auto mesh_result = mesh::io::load_from_path(mesh_path);
        REQUIRE(mesh_result.has_value());
        SimpleMesh &mesh = mesh_result.value();
        mesh.uvs.clear();
        mesh.texture = std::nullopt;

        const octree::Space space = octree::Space::earth();
        const auto mesh_bounds = calculate_bounds(mesh);
        const octree::Id id = space.find_smallest_node_encompassing_bounds(mesh_bounds).value();
        const auto child_ids = id.children().value();
        std::vector<octree::Bounds> child_bounds;
        child_bounds.reserve(child_ids.size());
        for (const auto &child_id : child_ids) {
            const auto bounds = space.get_node_bounds(child_id);
            child_bounds.push_back(bounds);
        }

        meter.measure([mesh, child_bounds] {
            std::vector<SimpleMesh> child_meshes;
            child_meshes.reserve(child_bounds.size());
            for (const auto &bounds : child_bounds) {
                const auto clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
                child_meshes.push_back(clipped_mesh);
            }
            return child_meshes;
        });
    };
}
#endif
