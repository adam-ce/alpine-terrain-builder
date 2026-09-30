/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include "../catch2_helpers.h"
#include "containers/HybridVector.h"
#include <memory>

TEST_CASE("HybridVector basic stack behavior") {
    HybridVector<int, 3> vec;

    CHECK(vec.empty());
    CHECK(vec.size() == 0);

    vec.push_back(1);
    vec.push_back(2);
    REQUIRE(vec.size() == 2);
    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);

    vec.pop_back();
    REQUIRE(vec.size() == 1);
    CHECK(vec[0] == 1);

    vec.resize(3);
    REQUIRE(vec.size() == 3);
    vec[1] = 10;
    vec[2] = 20;
    CHECK(vec[1] == 10);
    CHECK(vec[2] == 20);
}

TEST_CASE("HybridVector switches to heap when exceeding N") {
    HybridVector<int, 3> vec;

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    CHECK(vec.size() == 3);

    // Exceed stack capacity
    vec.push_back(4);
    REQUIRE(vec.size() == 4);
    CHECK(vec[0] == 1);
    CHECK(vec[3] == 4);

    // Resize beyond stack capacity triggers heap
    vec.resize(6);
    CHECK(vec.size() == 6);
}

TEST_CASE("HybridVector reserve works correctly") {
    HybridVector<int, 3> vec;
    vec.reserve(10);
    CHECK(vec.size() == 0);

    // Fill stack capacity
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    CHECK(vec.size() == 3);

    // Reserve beyond stack capacity triggers heap
    vec.reserve(5);
    vec.push_back(4);
    vec.push_back(5);
    CHECK(vec.size() == 5);
}

TEST_CASE("HybridVector accessors and at()") {
    HybridVector<int, 2> vec;
    vec.push_back(1);
    vec.push_back(2);

    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);
    CHECK(vec.at(0) == 1);
    CHECK(vec.at(1) == 2);
    REQUIRE_THROWS_AS(vec.at(2), std::out_of_range);
}

TEST_CASE("HybridVector iterators and span") {
    HybridVector<int, 3> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
        sum += *it;
    CHECK(sum == 6);

    sum = 0;
    for (int x : vec)
        sum += x;
    CHECK(sum == 6);

    std::span<int> s = vec;
    REQUIRE(s.size() == 3);
    CHECK(s[0] == 1);

    std::span<const int> cs = vec;
    CHECK(cs[2] == 3);
}

TEST_CASE("HybridVector works with move-only types") {
    HybridVector<std::unique_ptr<int>, 2> vec;
    vec.emplace_back(std::make_unique<int>(42));
    vec.emplace_back(std::make_unique<int>(99));

    CHECK(*vec[0] == 42);
    CHECK(*vec[1] == 99);

    // Exceed stack triggers heap
    vec.push_back(std::make_unique<int>(123));
    CHECK(vec.size() == 3);

    // Pop back
    vec.pop_back();
    REQUIRE(vec.size() == 2);
    CHECK(*vec[0] == 42);
    CHECK(*vec[1] == 99);

    // Resize triggers heap if needed
    vec.resize(5);
    REQUIRE(vec.size() == 5);
    REQUIRE(vec[0]);
    CHECK(*vec[0] == 42);
}
