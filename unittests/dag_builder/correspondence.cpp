#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "correspondence.h"
#include "range_utils.h"

namespace {

// A simplification step: the surface before it, and the triangles that replaced it.
struct Coarsening {
    std::vector<glm::dvec3> positions;
    std::vector<glm::uvec3> source_triangles;
    std::vector<glm::uvec3> output_triangles;
};

Correspondence run(const Coarsening &coarsening, const CorrespondenceOptions options = {}) {
    return find_source_triangles(coarsening.source_triangles, coarsening.output_triangles, coarsening.positions, options);
}

bool contains_source(
    const Correspondence &correspondence,
    const uint32_t output_triangle,
    const uint32_t source_triangle) {
    return contains(correspondence.segment(output_triangle), source_triangle);
}

// A grid coarsened to the two triangles of its diagonal.
Coarsening make_grid(const uint32_t side = 5) {
    Coarsening coarsening;
    const uint32_t last = side - 1;

    for (const uint32_t row : range(side)) {
        for (const uint32_t column : range(side)) {
            // Undulate, so a projection ignoring the output plane cannot pass; the corners carry
            // the output triangles and stay put.
            const bool is_output_corner = (row == 0 || row == last) && (column == 0 || column == last);
            const double height = is_output_corner ? 0.0 : 0.1 * (int32_t((row + column) % 3) - 1);
            coarsening.positions.push_back(glm::dvec3(column, row, height));
        }
    }

    for (const uint32_t row : range(last)) {
        for (const uint32_t column : range(last)) {
            const uint32_t lower_left = row * side + column;
            const uint32_t lower_right = lower_left + 1;
            const uint32_t upper_left = lower_left + side;
            const uint32_t upper_right = upper_left + 1;
            coarsening.source_triangles.push_back(glm::uvec3(lower_left, lower_right, upper_right));
            coarsening.source_triangles.push_back(glm::uvec3(lower_left, upper_right, upper_left));
        }
    }

    coarsening.output_triangles = {
        glm::uvec3(0, last, side * last),
        glm::uvec3(last, side * side - 1, side * last),
    };

    return coarsening;
}

// A bump sitting inside the output triangle, joined to it only by a detour outside the footprint.
Coarsening make_overhang() {
    Coarsening coarsening;
    coarsening.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(10.0, 0.0, 0.0),
        glm::dvec3(0.0, 10.0, 0.0),
        glm::dvec3(7.0, 6.0, 1.0), // detour, past the far edge
        glm::dvec3(6.0, 7.0, 1.0), // detour, past the far edge
        glm::dvec3(3.0, 3.0, 2.0), // bump, back inside the footprint
        glm::dvec3(4.0, 4.0, 2.0), // bump, back inside the footprint
    };
    coarsening.source_triangles = {
        glm::uvec3(0, 1, 3),
        glm::uvec3(1, 3, 2),
        glm::uvec3(2, 0, 4),
        glm::uvec3(3, 4, 5),
        glm::uvec3(4, 5, 6),
    };
    coarsening.output_triangles = {glm::uvec3(0, 1, 2)};
    return coarsening;
}

// The bump's triangles, which no walk can reach without first leaving the footprint.
constexpr uint32_t overhang_bump_root = 3;
constexpr uint32_t overhang_bump_tip = 4;

// A closed shell whose inset underside sits directly below the output triangle.
Coarsening make_thin_shell() {
    Coarsening coarsening;
    coarsening.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(4.0, 0.0, 0.0),
        glm::dvec3(4.0, 4.0, 0.0),
        glm::dvec3(0.0, 4.0, 0.0),
        // Inset and below, so the rim joining it to the top slopes outward and downward.
        glm::dvec3(0.5, 0.5, -0.5),
        glm::dvec3(3.5, 0.5, -0.5),
        glm::dvec3(3.5, 3.5, -0.5),
        glm::dvec3(0.5, 3.5, -0.5),
    };
    coarsening.source_triangles = {
        glm::uvec3(0, 1, 2), // top
        glm::uvec3(0, 2, 3), // top
        glm::uvec3(4, 6, 5), // underside, wound facing down
        glm::uvec3(4, 7, 6), // underside, wound facing down
        glm::uvec3(0, 4, 5), // rim
        glm::uvec3(0, 5, 1), // rim
    };
    coarsening.output_triangles = {glm::uvec3(0, 1, 2)};
    return coarsening;
}

// The underside triangles, directly beneath the output triangle but facing away from it.
constexpr uint32_t shell_underside_near = 2;
constexpr uint32_t shell_underside_far = 3;

} // namespace

TEST_CASE("find_source_triangles assigns every source triangle to a covering output triangle", "[dag_builder][correspondence]") {
    const Coarsening grid = make_grid();
    const Correspondence correspondence = run(grid);

    REQUIRE(correspondence.segment_count() == 2);

    for (const uint32_t source_triangle : range<uint32_t>(grid.source_triangles.size())) {
        REQUIRE((contains_source(correspondence, 0, source_triangle) || contains_source(correspondence, 1, source_triangle)));
    }
}

TEST_CASE("find_source_triangles keeps source triangles out of output triangles they do not cover", "[dag_builder][correspondence]") {
    const Correspondence correspondence = run(make_grid());

    // The corner cell nearest the origin lies strictly inside the first output triangle.
    CHECK(contains_source(correspondence, 0, 0));
    CHECK(contains_source(correspondence, 0, 1));
    CHECK_FALSE(contains_source(correspondence, 1, 0));
    CHECK_FALSE(contains_source(correspondence, 1, 1));

    // The opposite corner cell lies strictly inside the second.
    CHECK(contains_source(correspondence, 1, 30));
    CHECK(contains_source(correspondence, 1, 31));
    CHECK_FALSE(contains_source(correspondence, 0, 30));
    CHECK_FALSE(contains_source(correspondence, 0, 31));
}

TEST_CASE("find_source_triangles reaches geometry that leaves and re-enters the footprint", "[dag_builder][correspondence]") {
    const Correspondence correspondence = run(make_overhang());

    CHECK(contains_source(correspondence, 0, overhang_bump_root));
    CHECK(contains_source(correspondence, 0, overhang_bump_tip));
}

TEST_CASE("find_source_triangles stops at the slack bound", "[dag_builder][correspondence]") {
    const Correspondence correspondence = run(make_overhang(), CorrespondenceOptions{.slack_ratio = 0.0});

    CHECK_FALSE(contains_source(correspondence, 0, overhang_bump_root));
    CHECK_FALSE(contains_source(correspondence, 0, overhang_bump_tip));
}

TEST_CASE("find_source_triangles stops at the outside step bound", "[dag_builder][correspondence]") {
    const Correspondence correspondence = run(make_overhang(), CorrespondenceOptions{.max_steps_outside = 0});

    CHECK_FALSE(contains_source(correspondence, 0, overhang_bump_root));
    CHECK_FALSE(contains_source(correspondence, 0, overhang_bump_tip));
}

TEST_CASE("find_source_triangles does not cross onto the far side of a thin shell", "[dag_builder][correspondence]") {
    const Correspondence correspondence = run(make_thin_shell());

    REQUIRE(contains_source(correspondence, 0, 0));
    CHECK_FALSE(contains_source(correspondence, 0, shell_underside_near));
    CHECK_FALSE(contains_source(correspondence, 0, shell_underside_far));
}

TEST_CASE("find_source_triangles ignores components with no surviving vertex", "[dag_builder][correspondence]") {
    Coarsening grid = make_grid();
    const uint32_t detached_first = grid.positions.size();
    grid.positions.push_back(glm::dvec3(1.0, 1.0, 0.25));
    grid.positions.push_back(glm::dvec3(2.0, 1.0, 0.25));
    grid.positions.push_back(glm::dvec3(2.0, 2.0, 0.25));
    const uint32_t detached_triangle = grid.source_triangles.size();
    grid.source_triangles.push_back(glm::uvec3(detached_first, detached_first + 1, detached_first + 2));

    const Correspondence correspondence = run(grid);

    CHECK_FALSE(contains_source(correspondence, 0, detached_triangle));
    CHECK_FALSE(contains_source(correspondence, 1, detached_triangle));
}