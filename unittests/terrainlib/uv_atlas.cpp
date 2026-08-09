#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "enumerate.h"
#include "geometry/geometry.h"
#include "mesh/SimpleMesh.h"
#include "range_utils.h"
#include "uv/atlas.h"

namespace {

// Two triangles forming a unit square in the z = 0 plane.
mesh::Simple make_quad() {
    return mesh::Simple(
        {{0, 1, 2}, {0, 2, 3}},
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}});
}

// A closed unit cube, so charting has to cut seams to flatten it.
mesh::Simple make_cube() {
    std::vector<glm::dvec3> positions;
    for (const uint32_t corner : range(8u)) {
        positions.push_back(glm::dvec3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1));
    }
    return mesh::Simple(
        {
            {0, 2, 1},
            {1, 2, 3}, // z = 0
            {4, 5, 6},
            {5, 7, 6}, // z = 1
            {0, 1, 4},
            {1, 5, 4}, // y = 0
            {2, 6, 3},
            {3, 6, 7}, // y = 1
            {0, 4, 2},
            {2, 4, 6}, // x = 0
            {1, 3, 5},
            {3, 7, 5}, // x = 1
        },
        std::move(positions));
}

// Two triangles meeting at a single shared vertex, which is a non manifold vertex.
mesh::Simple make_bowtie() {
    return mesh::Simple(
        {{0, 1, 2}, {2, 3, 4}},
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {2, 1, 0}, {2, 2, 0}});
}

// Three triangles sharing one edge, which is a non manifold edge.
mesh::Simple make_fin() {
    return mesh::Simple(
        {{0, 1, 2}, {0, 3, 1}, {0, 1, 4}},
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}});
}

// A quad with one triangle hanging off a boundary edge, free on its other two.
mesh::Simple make_flap() {
    return mesh::Simple(
        {{0, 1, 2}, {0, 2, 3}, {1, 2, 4}},
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {2, 0.5, 0.5}});
}

// A quad, plus a triangle sharing nothing with it.
mesh::Simple make_detached() {
    return mesh::Simple(
        {{0, 1, 2}, {0, 2, 3}, {4, 5, 6}},
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {5, 5, 0}, {6, 5, 0}, {6, 6, 0}});
}

// The uv triangle a mesh triangle was laid out as.
radix::geometry::Triangle<2, double> uv_triangle(const uv::Atlas &atlas, const uint32_t triangle_index) {
    const glm::uvec3 triangle = atlas.triangles[triangle_index];
    return {atlas.uvs[triangle.x], atlas.uvs[triangle.y], atlas.uvs[triangle.z]};
}

double uv_area(const uv::Atlas &atlas, const uint32_t triangle_index) {
    const radix::geometry::Triangle<2, double> corners = uv_triangle(atlas, triangle_index);
    return std::abs(geometry::cross_2d(corners[1] - corners[0], corners[2] - corners[0])) / 2.0;
}

// The source vertices of a laid out triangle, which survive duplication.
glm::uvec3 source_triangle(const uv::Atlas &atlas, const uint32_t triangle_index) {
    const glm::uvec3 triangle = atlas.triangles[triangle_index];
    return {
        atlas.vertex_map[triangle.x],
        atlas.vertex_map[triangle.y],
        atlas.vertex_map[triangle.z],
    };
}

// Every triangle carries uv area, and addresses the atlas within its bounds.
void check_fully_mapped(const uv::Atlas &atlas, const mesh::Simple &mesh) {
    CHECK(atlas.unmapped_triangles.empty());
    REQUIRE(atlas.triangles.size() == mesh.face_count());
    REQUIRE(atlas.uvs.size() == atlas.vertex_map.size());

    for (const auto [vertex, uv] : enumerate(atlas.uvs)) {
        REQUIRE(uv.x >= 0.0);
        REQUIRE(uv.y >= 0.0);
        REQUIRE(uv.x <= 1.0);
        REQUIRE(uv.y <= 1.0);
        REQUIRE(atlas.vertex_map[vertex] < mesh.vertex_count());
    }
    for (const uint32_t triangle_index : range(atlas.triangles.size())) {
        REQUIRE(uv_area(atlas, triangle_index) > 0.0);
    }
}

} // namespace

TEST_CASE("build_atlas lays out a flat quad as one chart", "[terrainlib][uv][atlas]") {
    const mesh::Simple mesh = make_quad();

    const uv::Atlas atlas = uv::build_atlas(mesh);

    check_fully_mapped(atlas, mesh);
    CHECK(atlas.chart_count == 1);
    // A plane needs no cut, so nothing is duplicated.
    CHECK(atlas.uvs.size() == mesh.vertex_count());
}

TEST_CASE("build_atlas keeps input triangle order", "[terrainlib][uv][atlas]") {
    const mesh::Simple mesh = make_cube();

    const uv::Atlas atlas = uv::build_atlas(mesh);

    REQUIRE(atlas.triangles.size() == mesh.face_count());
    for (const auto [triangle_index, triangle] : enumerate(mesh.triangles)) {
        const glm::uvec3 sources = source_triangle(atlas, triangle_index);
        // Duplication can rotate a triangle, but not change which vertices it is over.
        REQUIRE(glm::min(glm::min(sources.x, sources.y), sources.z) == glm::min(glm::min(triangle.x, triangle.y), triangle.z));
        REQUIRE(sources.x + sources.y + sources.z == triangle.x + triangle.y + triangle.z);
    }
}

TEST_CASE("build_atlas cuts a closed cube without manifold repair", "[terrainlib][uv][atlas]") {
    const mesh::Simple mesh = make_cube();

    const uv::Atlas atlas = uv::build_atlas(mesh);

    check_fully_mapped(atlas, mesh);
    // A closed surface cannot be one chart, and the seams have to duplicate vertices.
    CHECK(atlas.chart_count > 1);
    CHECK(atlas.uvs.size() > mesh.vertex_count());
}

TEST_CASE("build_atlas unwraps non manifold input", "[terrainlib][uv][atlas]") {
    const auto [name, mesh] = GENERATE(
        std::pair{"bowtie", make_bowtie()},
        std::pair{"fin", make_fin()},
        std::pair{"flap", make_flap()},
        std::pair{"detached", make_detached()});
    INFO("mesh: " << name);

    const uv::Atlas atlas = uv::build_atlas(mesh);

    check_fully_mapped(atlas, mesh);
    CHECK(atlas.chart_count >= 1);
}

TEST_CASE("build_atlas reports degenerate triangles instead of dropping them", "[terrainlib][uv][atlas]") {
    // The two shapes xatlas refuses: a repeated corner, and three distinct but collinear ones.
    const auto [name, degenerate] = GENERATE(
        std::pair{"repeated corner", glm::uvec3(1, 4, 4)},
        std::pair{"collinear corners", glm::uvec3(0, 1, 4)});
    INFO("degenerate: " << name);

    mesh::Simple mesh = make_quad();
    mesh.positions.push_back(glm::dvec3(2, 0, 0));
    mesh.triangles.push_back(degenerate);
    mesh.triangles.push_back(glm::uvec3(0, 1, 3));

    const uv::Atlas atlas = uv::build_atlas(mesh);

    REQUIRE(atlas.triangles.size() == mesh.face_count());
    CHECK(atlas.unmapped_triangles == std::vector<uint32_t>{2});
    CHECK(uv_area(atlas, 2) == 0.0);
    // The rest is laid out as if the sliver were not there.
    CHECK(uv_area(atlas, 0) > 0.0);
    CHECK(uv_area(atlas, 3) > 0.0);
}

TEST_CASE("build_atlas normalises the uvs against the packed size", "[terrainlib][uv][atlas]") {
    const mesh::Simple mesh = make_cube();

    const uv::Atlas atlas = uv::build_atlas(mesh);

    for (const glm::dvec2 &uv : atlas.uvs) {
        REQUIRE(uv.x >= 0.0);
        REQUIRE(uv.y >= 0.0);
        REQUIRE(uv.x <= 1.0);
        REQUIRE(uv.y <= 1.0);
    }
}

TEST_CASE("build_atlas tolerates an empty mesh", "[terrainlib][uv][atlas]") {
    const uv::Atlas atlas = uv::build_atlas(mesh::Simple{});

    CHECK(atlas.triangles.empty());
    CHECK(atlas.uvs.empty());
    CHECK(atlas.chart_count == 0);
}