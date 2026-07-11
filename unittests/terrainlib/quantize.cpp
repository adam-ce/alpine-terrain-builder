#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "quantize.h"

TEST_CASE("quantize_floor scalar", "[quantize]") {
    CHECK(quantize_floor(3, 2) == 2);
    CHECK(quantize_floor(-2, 2) == -2);
    CHECK(quantize_floor(-1, 2) == -2);
    CHECK(quantize_floor(-3, 2) == -4);

    CHECK(quantize_floor(3.0, 2.0) == 2.0);
    CHECK(quantize_floor(-1.0, 2.0) == -2.0);
}

TEST_CASE("quantize_floor vector", "[quantize]") {
    CHECK(quantize_floor(glm::ivec2(3, -1), 2) == glm::ivec2(2, -2));
    CHECK(quantize_floor(glm::dvec2(3.0, -1.0), 2.0) == glm::dvec2(2.0, -2.0));
}
