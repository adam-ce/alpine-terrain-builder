#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "texture_sizing.h"

TEST_CASE("compute_target_size returns the demanded texels when the uvs cover everything", "[dag_builder][texture_sizing]") {
    CHECK(compute_target_size({.texels = 10000, .coverage = 1.0}, 1.0) == glm::uvec2(100, 100));
}

TEST_CASE("compute_target_size grows the texture when the uvs cover only part of it", "[dag_builder][texture_sizing]") {
    // Half the coverage needs twice the area, so 20000 texels.
    CHECK(compute_target_size({.texels = 10000, .coverage = 0.5}, 1.0) == glm::uvec2(142, 142));
}

TEST_CASE("compute_target_size shapes the texture by the demanded aspect", "[dag_builder][texture_sizing]") {
    CHECK(compute_target_size({.texels = 10000, .coverage = 1.0}, 4.0) == glm::uvec2(200, 50));
}

TEST_CASE("compute_target_size caps the growth of a barely covered texture", "[dag_builder][texture_sizing]") {
    const glm::uvec2 clamped = compute_target_size({.texels = 10000, .coverage = 0.1}, 1.0);

    CHECK(compute_target_size({.texels = 10000, .coverage = 0.01}, 1.0) == clamped);
    CHECK(compute_target_size({.texels = 10000, .coverage = 0.0}, 1.0) == clamped);
}
