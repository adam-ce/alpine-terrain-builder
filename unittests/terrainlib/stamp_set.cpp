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
#include <limits>

#include "../catch2_helpers.h"

#include "containers/StampSet.h"

TEST_CASE("StampSet starts empty", "[terrainlib][stampset]") {
    const StampSet set(4);

    REQUIRE(set.capacity() == 4);
    for (uint32_t index = 0; index < 4; index++) {
        CHECK_FALSE(set.contains(index));
    }
}

TEST_CASE("StampSet::insert reports only the first insert", "[terrainlib][stampset]") {
    StampSet set(4);

    CHECK(set.insert(2));
    CHECK(set.contains(2));
    CHECK_FALSE(set.insert(2));
    CHECK_FALSE(set.contains(3));
}

TEST_CASE("StampSet::reset empties the set", "[terrainlib][stampset]") {
    StampSet set(4);
    set.insert(1);
    set.insert(3);

    set.reset(4);

    CHECK_FALSE(set.contains(1));
    CHECK_FALSE(set.contains(3));
    CHECK(set.insert(1));
}

TEST_CASE("StampSet::reset resizes without carrying entries over", "[terrainlib][stampset]") {
    StampSet set(2);
    set.insert(0);
    set.insert(1);

    set.reset(5);

    REQUIRE(set.capacity() == 5);
    for (uint32_t index = 0; index < 5; index++) {
        CHECK_FALSE(set.contains(index));
    }
}

TEST_CASE("StampSet survives running out of generations", "[terrainlib][stampset]") {
    // A narrow generation reaches the wraparound in a few hundred resets rather than billions.
    StampSet_<uint8_t> set(3);

    for (uint32_t i = 0; i < 4 * std::numeric_limits<uint8_t>::max(); i++) {
        set.reset(3);
        REQUIRE_FALSE(set.contains(1));
        REQUIRE(set.insert(1));
        REQUIRE(set.contains(1));
        REQUIRE_FALSE(set.contains(0));
    }
}