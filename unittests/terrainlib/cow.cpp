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

#include "../catch2_helpers.h"
#include "containers/Cow.h"

#include <string>
#include <vector>

TEST_CASE("Cow: construct from owned rvalue") {
    std::vector<int> v = {1, 2, 3};
    Cow<std::vector<int>> cow(std::move(v));

    CHECK(cow.is_owned());
    REQUIRE_FALSE(cow.is_ref());
    CHECK(cow.get() == std::vector<int>{1, 2, 3});
}

TEST_CASE("Cow: construct from reference") {
    std::vector<int> original = {10, 20, 30};
    Cow<std::vector<int>> cow(original);

    CHECK(cow.is_ref());
    REQUIRE_FALSE(cow.is_owned());
    CHECK(cow.get() == std::vector<int>{10, 20, 30});

    // Modifying the original should be visible through the Cow
    original.push_back(40);
    CHECK(cow.get().size() == 4);
    CHECK(cow.get().back() == 40);
}

TEST_CASE("Cow: from_owned factory") {
    std::string s = "hello";
    auto cow = Cow<std::string>::from_owned(std::move(s));

    CHECK(cow.is_owned());
    REQUIRE_FALSE(cow.is_ref());
    CHECK(cow.get() == "hello");
}

TEST_CASE("Cow: from_ref factory") {
    std::string original = "world";
    auto cow = Cow<std::string>::from_ref(std::ref(original));

    CHECK(cow.is_ref());
    REQUIRE_FALSE(cow.is_owned());
    CHECK(cow.get() == "world");

    // Modifying the original should be visible through the Cow
    original += "!";
    CHECK(cow.get() == "world!");
}

TEST_CASE("Cow: operator* and operator->") {
    SECTION("owned") {
        std::vector<int> v = {1, 2, 3};
        Cow<std::vector<int>> cow(std::move(v));

        CHECK(*cow == std::vector<int>{1, 2, 3});
        CHECK(cow->size() == 3);
    }

    SECTION("ref") {
        std::vector<int> original = {4, 5};
        Cow<std::vector<int>> cow(original);

        CHECK(*cow == std::vector<int>{4, 5});
        CHECK(cow->size() == 2);
    }
}

TEST_CASE("Cow: implicit conversion to T&") {
    std::vector<int> v = {7, 8, 9};
    Cow<std::vector<int>> cow(std::move(v));

    const std::vector<int>& ref = cow;
    CHECK(ref == std::vector<int>{7, 8, 9});
}

TEST_CASE("Cow: copy owned is independent") {
    std::vector<int> v = {1, 2, 3};
    Cow<std::vector<int>> original(std::move(v));

    Cow<std::vector<int>> copy = original;
    CHECK(copy.is_owned());
    CHECK(copy.get() == std::vector<int>{1, 2, 3});

    // Modifying the copy should not affect the original
    copy.get().push_back(4);
    CHECK(original.get() == std::vector<int>{1, 2, 3});
    CHECK(copy.get() == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("Cow: copy ref still references original") {
    std::vector<int> source = {10, 20};
    Cow<std::vector<int>> cow(source);

    Cow<std::vector<int>> copy = cow;
    CHECK(copy.is_ref());

    // Both the copy and the original Cow should see mutations to the source
    source.push_back(30);
    CHECK(cow.get().size() == 3);
    CHECK(copy.get().size() == 3);
}

TEST_CASE("Cow: move") {
    std::vector<int> v = {1, 2, 3};
    Cow<std::vector<int>> original(std::move(v));

    Cow<std::vector<int>> moved = std::move(original);
    CHECK(moved.is_owned());
    CHECK(moved.get() == std::vector<int>{1, 2, 3});
}

TEST_CASE("Cow: conversion to Cow<const T> via rvalue") {
    SECTION("owned case moves") {
        std::vector<int> v = {1, 2, 3};
        Cow<std::vector<int>> cow(std::move(v));

        Cow<const std::vector<int>> const_cow = std::move(cow);
        CHECK(const_cow.is_owned());
        CHECK(const_cow.get() == std::vector<int>{1, 2, 3});
    }

    SECTION("ref case creates const ref") {
        std::vector<int> source = {4, 5, 6};
        Cow<std::vector<int>> cow(source);

        Cow<const std::vector<int>> const_cow = std::move(cow);
        CHECK(const_cow.is_ref());
        CHECK(const_cow.get() == std::vector<int>{4, 5, 6});

        // Mutations to source should still be visible
        source.push_back(7);
        CHECK(const_cow.get().size() == 4);
    }
}
