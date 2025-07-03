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
#include "mesh/utils.h"
#include "mesh/validate.h"
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

void run_checks(const SimpleMesh &mesh, const SimpleMesh &clipped_mesh, const radix::geometry::Aabb3d &bounds) {
    mesh::validate(clipped_mesh);

    // Check that no vertices are outside the bounds
    for (const auto &position : clipped_mesh.positions) {
        // INFO(fmt::format("Vertex: {}", position));
        REQUIRE(bounds.contains_inclusive(position));
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

        REQUIRE(should_be_kept == found);
    }
}

TEST_CASE("mesh::clip_on_bounds") {
    const std::filesystem::path mesh_path = ATB_TEST_DATA_DIR "/meshes/6857.terrain";
    auto mesh_result = mesh::io::load_from_path(mesh_path);
    REQUIRE(mesh_result.has_value());
    SimpleMesh &mesh = mesh_result.value();
    mesh.uvs.clear(); // Ensure no UVs are present, as clipping with UVs is not supported yet.
    mesh.texture = std::nullopt;

    // Create cube mesh
    mesh.positions = {
        glm::dvec3(-1.0, -1.0, -1.0), // 0
        glm::dvec3(1.0, -1.0, -1.0),  // 1
        glm::dvec3(1.0, 1.0, -1.0),   // 2
        glm::dvec3(-1.0, 1.0, -1.0),  // 3
        glm::dvec3(-1.0, -1.0, 1.0),  // 4
        glm::dvec3(1.0, -1.0, 1.0),   // 5
        glm::dvec3(1.0, 1.0, 1.0),    // 6
        glm::dvec3(-1.0, 1.0, 1.0)    // 7
    };

    mesh.triangles = {
        // Bottom face
        glm::uvec3(0, 1, 2),
        glm::uvec3(0, 2, 3),
        // Top face
        glm::uvec3(4, 5, 6),
        glm::uvec3(4, 6, 7),
        // Front face
        glm::uvec3(0, 1, 5),
        glm::uvec3(0, 5, 4),
        // Back face
        glm::uvec3(3, 2, 6),
        glm::uvec3(3, 6, 7),
        // Left face
        glm::uvec3(0, 3, 7),
        glm::uvec3(0, 7, 4),
        // Right face
        glm::uvec3(1, 2, 6),
        glm::uvec3(1, 6, 5)};

    mesh::validate(mesh);

    const std::array<radix::geometry::Aabb3d, 11> bounds_array = {
        radix::geometry::Aabb3d(glm::dvec3(-2.0, -2.0, -2.0), glm::dvec3(2.0, 2.0, 2.0)),
        radix::geometry::Aabb3d(glm::dvec3(2.0, 2.0, 2.0), glm::dvec3(3.0, 3.0, 3.0)),
        radix::geometry::Aabb3d(glm::dvec3(-1.0, -1.0, -1.0), glm::dvec3(1.0, 1.0, 1.0)),
        radix::geometry::Aabb3d(glm::dvec3(0.5, 0.5, 0.5), glm::dvec3(2.0, 2.0, 2.0)),
        radix::geometry::Aabb3d(glm::dvec3(-0.5, -0.5, -0.5), glm::dvec3(0.5, 0.5, 0.5)),
        // tiny slice of one side:
        radix::geometry::Aabb3d(glm::dvec3(-2.0, -2.0, -2.0), glm::dvec3(-0.99, 2.0, 2.0)), // -x
        radix::geometry::Aabb3d(glm::dvec3(0.99, -2.0, -2.0), glm::dvec3(2.0, 2.0, 2.0)),   // +x
        radix::geometry::Aabb3d(glm::dvec3(-2.0, -2.0, -2.0), glm::dvec3(2.0, -0.99, 2.0)), // -y
        radix::geometry::Aabb3d(glm::dvec3(-2.0, 0.99, -2.0), glm::dvec3(2.0, 2.0, 2.0)),   // +y
        radix::geometry::Aabb3d(glm::dvec3(-2.0, -2.0, -2.0), glm::dvec3(2.0, 2.0, -0.99)), // -z
        radix::geometry::Aabb3d(glm::dvec3(-2.0, -2.0, 0.99), glm::dvec3(2.0, 2.0, 2.0))    // +z
    };

    const radix::geometry::Aabb3d mesh_bounds = calculate_bounds(mesh);
    const glm::dvec3 mesh_centre = (mesh_bounds.max + mesh_bounds.min) / glm::dvec3(2);
    const glm::dvec3 mesh_extends = mesh_bounds.size() / glm::dvec3(2);
    for (const auto &relative_bounds : bounds_array) {
        CAPTURE(relative_bounds);
        const radix::geometry::Aabb3d bounds{
            mesh_centre + relative_bounds.min * mesh_extends,
            mesh_centre + relative_bounds.max * mesh_extends};
        CAPTURE(bounds);
        SimpleMesh clipped_mesh = mesh::clip_on_bounds(mesh, bounds);
        run_checks(mesh, clipped_mesh, bounds);
    }
}

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
