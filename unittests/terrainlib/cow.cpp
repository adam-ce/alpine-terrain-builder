#include "../catch2_helpers.h"
#include "Cow.h"

#include <string>
#include <vector>

TEST_CASE("Cow: construct from owned rvalue") {
    std::vector<int> v = {1, 2, 3};
    Cow<std::vector<int>> cow(std::move(v));

    REQUIRE(cow.is_owned());
    REQUIRE_FALSE(cow.is_ref());
    REQUIRE(cow.get() == std::vector<int>{1, 2, 3});
}

TEST_CASE("Cow: construct from reference") {
    std::vector<int> original = {10, 20, 30};
    Cow<std::vector<int>> cow(original);

    REQUIRE(cow.is_ref());
    REQUIRE_FALSE(cow.is_owned());
    REQUIRE(cow.get() == std::vector<int>{10, 20, 30});

    // Modifying the original should be visible through the Cow
    original.push_back(40);
    REQUIRE(cow.get().size() == 4);
    REQUIRE(cow.get().back() == 40);
}

TEST_CASE("Cow: from_owned factory") {
    std::string s = "hello";
    auto cow = Cow<std::string>::from_owned(std::move(s));

    REQUIRE(cow.is_owned());
    REQUIRE_FALSE(cow.is_ref());
    REQUIRE(cow.get() == "hello");
}

TEST_CASE("Cow: from_ref factory") {
    std::string original = "world";
    auto cow = Cow<std::string>::from_ref(std::ref(original));

    REQUIRE(cow.is_ref());
    REQUIRE_FALSE(cow.is_owned());
    REQUIRE(cow.get() == "world");

    // Modifying the original should be visible through the Cow
    original += "!";
    REQUIRE(cow.get() == "world!");
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
    REQUIRE(ref == std::vector<int>{7, 8, 9});
}

TEST_CASE("Cow: copy owned is independent") {
    std::vector<int> v = {1, 2, 3};
    Cow<std::vector<int>> original(std::move(v));

    Cow<std::vector<int>> copy = original;
    REQUIRE(copy.is_owned());
    REQUIRE(copy.get() == std::vector<int>{1, 2, 3});

    // Modifying the copy should not affect the original
    copy.get().push_back(4);
    REQUIRE(original.get() == std::vector<int>{1, 2, 3});
    REQUIRE(copy.get() == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("Cow: copy ref still references original") {
    std::vector<int> source = {10, 20};
    Cow<std::vector<int>> cow(source);

    Cow<std::vector<int>> copy = cow;
    REQUIRE(copy.is_ref());

    // Both the copy and the original Cow should see mutations to the source
    source.push_back(30);
    REQUIRE(cow.get().size() == 3);
    REQUIRE(copy.get().size() == 3);
}

TEST_CASE("Cow: move") {
    std::vector<int> v = {1, 2, 3};
    Cow<std::vector<int>> original(std::move(v));

    Cow<std::vector<int>> moved = std::move(original);
    REQUIRE(moved.is_owned());
    REQUIRE(moved.get() == std::vector<int>{1, 2, 3});
}

TEST_CASE("Cow: conversion to Cow<const T> via rvalue") {
    SECTION("owned case moves") {
        std::vector<int> v = {1, 2, 3};
        Cow<std::vector<int>> cow(std::move(v));

        Cow<const std::vector<int>> const_cow = std::move(cow);
        REQUIRE(const_cow.is_owned());
        REQUIRE(const_cow.get() == std::vector<int>{1, 2, 3});
    }

    SECTION("ref case creates const ref") {
        std::vector<int> source = {4, 5, 6};
        Cow<std::vector<int>> cow(source);

        Cow<const std::vector<int>> const_cow = std::move(cow);
        REQUIRE(const_cow.is_ref());
        REQUIRE(const_cow.get() == std::vector<int>{4, 5, 6});

        // Mutations to source should still be visible
        source.push_back(7);
        REQUIRE(const_cow.get().size() == 4);
    }
}
