/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <cstdint>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "numeric/quantize.h"

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

TEST_CASE("quantize_index scalar", "[quantize]") {
    CHECK(quantize_index(3, 2) == 1);
    CHECK(quantize_index(-2, 2) == -1);
    CHECK(quantize_index(-1, 2) == -1);
    CHECK(quantize_index(-3, 2) == -2);

    CHECK(quantize_index(3.0, 2.0) == 1);
    CHECK(quantize_index(-1.0, 2.0) == -1);
}

TEST_CASE("quantize_index vector", "[quantize]") {
    CHECK(quantize_index(glm::ivec2(3, -1), 2) == glm::i64vec2(1, -1));
    CHECK(quantize_index(glm::dvec2(3.0, -1.0), 2.0) == glm::i64vec2(1, -1));
}

// Neighbouring points must land in the same cell as long as they share a quantized corner.
TEST_CASE("quantize_index agrees with quantize_floor at earth-centered magnitudes", "[quantize]") {
    constexpr double epsilon = 1.28592041015625;
    constexpr double start = 4732875.8476;
    constexpr uint32_t sample_count = 1000;

    uint32_t disagreements = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        const double sample = start + i * 0.7;
        if (quantize_index(sample, epsilon) * epsilon != quantize_floor(sample, epsilon)) {
            disagreements++;
        }
    }

    CHECK(disagreements == 0);
}
