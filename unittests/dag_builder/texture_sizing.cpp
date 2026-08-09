#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "texture_sizing.h"

TEST_CASE("compute_target_size returns the demanded texels when the uvs cover everything", "[dag_builder][texture_sizing]") {
    CHECK(compute_target_size({.texels = 10000, .utilization = 1.0}, 1.0) == glm::uvec2(100, 100));
}

TEST_CASE("compute_target_size grows the texture when the uvs cover only part of it", "[dag_builder][texture_sizing]") {
    // Half the coverage needs twice the area, so 20000 texels.
    CHECK(compute_target_size({.texels = 10000, .utilization = 0.5}, 1.0) == glm::uvec2(142, 142));
}

TEST_CASE("compute_target_size shapes the texture by the demanded aspect", "[dag_builder][texture_sizing]") {
    CHECK(compute_target_size({.texels = 10000, .utilization = 1.0}, 4.0) == glm::uvec2(200, 50));
}

TEST_CASE("compute_target_size caps the growth of a barely covered texture", "[dag_builder][texture_sizing]") {
    const glm::uvec2 clamped = compute_target_size({.texels = 10000, .utilization = 0.1}, 1.0);

    CHECK(compute_target_size({.texels = 10000, .utilization = 0.01}, 1.0) == clamped);
    CHECK(compute_target_size({.texels = 10000, .utilization = 0.0}, 1.0) == clamped);
}

namespace {

BakePlan make_plan(const glm::uvec2 size) {
    return BakePlan{.clusters = {}, .size = size};
}

} // namespace

TEST_CASE("rescale_to_fit_budget leaves sizes alone below the budget", "[dag_builder][texture_sizing]") {
    std::vector<BakePlan> plans{make_plan({100, 100}), make_plan({200, 200})};

    rescale_to_fit_budget(plans, 100000);

    CHECK(plans[0].size == glm::uvec2(100, 100));
    CHECK(plans[1].size == glm::uvec2(200, 200));
}

TEST_CASE("rescale_to_fit_budget scales every size by the same factor", "[dag_builder][texture_sizing]") {
    std::vector<BakePlan> plans{make_plan({100, 100}), make_plan({200, 200})};

    // Half the area of the requested 50000 texels, so every side scales by 0.5.
    rescale_to_fit_budget(plans, 12500);

    CHECK(plans[0].size == glm::uvec2(50, 50));
    CHECK(plans[1].size == glm::uvec2(100, 100));
}

TEST_CASE("rescale_to_fit_budget keeps the aspect of a non square size", "[dag_builder][texture_sizing]") {
    std::vector<BakePlan> plans{make_plan({400, 100})};

    rescale_to_fit_budget(plans, 10000);

    CHECK(plans[0].size == glm::uvec2(200, 50));
}

TEST_CASE("rescale_to_fit_budget floors sizes at one texel", "[dag_builder][texture_sizing]") {
    std::vector<BakePlan> plans{make_plan({64, 64})};

    rescale_to_fit_budget(plans, 1);

    CHECK(plans[0].size == glm::uvec2(1, 1));
}

// Rounding up after the uniform scale can leave the total above the budget. This is
// accepted: max_node_texels is a soft cap, not a guarantee.
TEST_CASE("rescale_to_fit_budget can overshoot the budget on many small sizes", "[dag_builder][texture_sizing]") {
    std::vector<BakePlan> plans(10, make_plan({3, 3}));

    rescale_to_fit_budget(plans, 45);

    CHECK(plans[0].size == glm::uvec2(3, 3));
}
