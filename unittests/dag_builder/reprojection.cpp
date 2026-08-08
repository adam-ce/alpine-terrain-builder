#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "atlas/reprojection.h"
#include "range_utils.h"

namespace {

// A surface and the atlas minted on it, held together so the fixtures read as one thing.
struct Bake {
    std::vector<glm::dvec3> positions;
    std::vector<glm::uvec3> triangles;
    std::vector<SourceMapping> mapping;
    std::vector<std::vector<glm::dvec2>> uv_maps;

    uv::Atlas atlas;
    Correspondence correspondence;

    SourceSurface source() const {
        return SourceSurface{this->triangles, this->positions, this->mapping, this->uv_maps};
    }

    std::vector<ReprojectionTriangle> run() const {
        return build_reprojection_triangles(this->atlas, this->correspondence, this->source(), this->positions);
    }
};

// An atlas laying the given triangles out over the whole unit square, without duplication.
uv::Atlas make_atlas(std::vector<glm::uvec3> triangles, const std::vector<glm::dvec2> &uvs) {
    uv::Atlas atlas;
    atlas.triangles = std::move(triangles);
    atlas.uvs = uvs;
    atlas.vertex_map = std::vector<uint32_t>(uvs.size());
    for (const uint32_t vertex : range<uint32_t>(uvs.size())) {
        atlas.vertex_map[vertex] = vertex;
    }
    atlas.size = glm::uvec2(64);
    atlas.chart_count = 1;
    return atlas;
}

Correspondence make_correspondence(const std::vector<std::vector<uint32_t>> &per_output) {
    Correspondence correspondence;
    for (const std::vector<uint32_t> &sources : per_output) {
        correspondence.push_new_segment(sources);
    }
    return correspondence;
}

// One output triangle standing in for exactly one coplanar source triangle.
Bake make_identity() {
    Bake bake;
    bake.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    bake.triangles = {{0, 1, 2}};
    bake.uv_maps = {{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}}};
    bake.mapping = {SourceMapping{.map_index = 0, .uvs = {0, 1, 2}}};

    bake.atlas = make_atlas({{0, 1, 2}}, {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}});
    bake.correspondence = make_correspondence({{0}});
    return bake;
}

// One output triangle covering a square split into two coplanar source triangles.
Bake make_two_sources() {
    Bake bake;
    // The output triangle is the lower left half of the unit square, and the sources halve it again.
    bake.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0.5, 0.5, 0}};
    bake.triangles = {{0, 1, 3}, {0, 3, 2}};
    bake.uv_maps = {
        {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {0.5, 0.5}},
        {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {0.5, 0.5}},
    };
    bake.mapping = {
        SourceMapping{.map_index = 0, .uvs = {0, 1, 3}},
        SourceMapping{.map_index = 1, .uvs = {0, 3, 2}},
    };

    bake.atlas = make_atlas({{0, 1, 2}}, {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {0.5, 0.5}});
    bake.correspondence = make_correspondence({{0, 1}});
    return bake;
}

double area_of(const std::array<glm::dvec2, 3> &triangle) {
    return std::abs(cross_2d(triangle[1] - triangle[0], triangle[2] - triangle[0])) / 2.0;
}

double covered_area(const std::vector<ReprojectionTriangle> &triangles) {
    double area = 0.0;
    for (const ReprojectionTriangle &triangle : triangles) {
        area += area_of(triangle.target_uvs);
    }
    return area;
}

} // namespace

TEST_CASE("build_reprojection maps a coplanar source onto itself", "[dagbuilder][reprojection]") {
    const Bake bake = make_identity();

    const std::vector<ReprojectionTriangle> reprojection = bake.run();

    REQUIRE(reprojection.size() == 1);
    CHECK(reprojection[0].source_image_index == 0);
    // Source and target uv spaces coincide here, so the mapping has to be the identity.
    for (const uint8_t corner : range<uint8_t>(3)) {
        CHECK(reprojection[0].source_uvs[corner].x == Catch::Approx(reprojection[0].target_uvs[corner].x));
        CHECK(reprojection[0].source_uvs[corner].y == Catch::Approx(reprojection[0].target_uvs[corner].y));
    }
}

TEST_CASE("build_reprojection splits an output triangle between its sources", "[dagbuilder][reprojection]") {
    const Bake bake = make_two_sources();

    const std::vector<ReprojectionTriangle> reprojection = bake.run();

    REQUIRE(reprojection.size() >= 2);
    // The pieces tile the output triangle exactly once.
    CHECK(covered_area(reprojection) == Catch::Approx(0.5));

    uint32_t from_first = 0;
    for (const ReprojectionTriangle &triangle : reprojection) {
        from_first += triangle.source_image_index == 0 ? 1 : 0;
    }
    CHECK(from_first > 0);
    CHECK(from_first < reprojection.size());
}

TEST_CASE("build_reprojection emits nothing for an unmapped triangle", "[dagbuilder][reprojection]") {
    Bake bake = make_identity();
    bake.atlas.unmapped_triangles = {0};

    CHECK(bake.run().empty());
}

TEST_CASE("build_reprojection ignores a source that misses the output triangle", "[dagbuilder][reprojection]") {
    Bake bake = make_identity();
    // A second source triangle well outside the output triangle, still listed as a candidate.
    bake.positions.insert(bake.positions.end(), {{5, 5, 0}, {6, 5, 0}, {5, 6, 0}});
    bake.triangles.push_back({3, 4, 5});
    bake.uv_maps.push_back({{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}});
    bake.mapping.push_back(SourceMapping{.map_index = 1, .uvs = {0, 1, 2}});
    bake.correspondence = make_correspondence({{0, 1}});

    const std::vector<ReprojectionTriangle> reprojection = bake.run();

    REQUIRE(reprojection.size() == 1);
    CHECK(reprojection[0].source_image_index == 0);
}

TEST_CASE("build_reprojection orders overlapping sources nearest last", "[dagbuilder][reprojection]") {
    Bake bake = make_identity();
    // A second source covering the same footprint, but lifted off the output plane.
    bake.positions.insert(bake.positions.end(), {{0, 0, 2}, {1, 0, 2}, {0, 1, 2}});
    bake.triangles.push_back({3, 4, 5});
    bake.uv_maps.push_back({{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}});
    bake.mapping.push_back(SourceMapping{.map_index = 1, .uvs = {0, 1, 2}});
    bake.correspondence = make_correspondence({{1, 0}});

    const std::vector<ReprojectionTriangle> reprojection = bake.run();

    REQUIRE(reprojection.size() == 2);
    // The far one is drawn first so the coplanar one wins, whatever order the candidates came in.
    CHECK(reprojection[0].source_image_index == 1);
    CHECK(reprojection[1].source_image_index == 0);
}
