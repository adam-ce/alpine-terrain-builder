#include "../catch2_helpers.h"
#include "TinyVector.h"

#include <utility>

TEST_CASE("TinyVector default construction is empty") {
    TinyVector<int> vec;

    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.data() == nullptr);
    REQUIRE(vec.begin() == vec.end());
}

TEST_CASE("TinyVector push one element stays inline") {
    TinyVector<int> vec;
    vec.push_back(42);

    REQUIRE(vec.size() == 1);
    REQUIRE_FALSE(vec.empty());
    REQUIRE(vec[0] == 42);
    REQUIRE(vec.data() != nullptr);
}

TEST_CASE("TinyVector push two elements promotes to vector") {
    TinyVector<int> vec;
    vec.push_back(10);
    vec.push_back(20);

    REQUIRE(vec.size() == 2);
    REQUIRE(vec[0] == 10);
    REQUIRE(vec[1] == 20);
}

TEST_CASE("TinyVector push three elements") {
    TinyVector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
}

TEST_CASE("TinyVector pop_back from size 2 to 1") {
    TinyVector<int> vec;
    vec.push_back(5);
    vec.push_back(6);
    REQUIRE(vec.size() == 2);

    vec.pop_back();
    REQUIRE(vec.size() == 1);
    REQUIRE(vec[0] == 5);
}

TEST_CASE("TinyVector pop_back from size 1 to 0") {
    TinyVector<int> vec;
    vec.push_back(99);

    vec.pop_back();
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.empty());
}

TEST_CASE("TinyVector pop_back on empty throws") {
    TinyVector<int> vec;

    REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
}

TEST_CASE("TinyVector clear from various sizes") {
    SECTION("clear from size 0") {
        TinyVector<int> vec;
        vec.clear();
        REQUIRE(vec.empty());
    }

    SECTION("clear from size 1") {
        TinyVector<int> vec;
        vec.push_back(1);
        vec.clear();
        REQUIRE(vec.empty());
        REQUIRE(vec.size() == 0);
    }

    SECTION("clear from size 3") {
        TinyVector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.clear();
        REQUIRE(vec.empty());
        REQUIRE(vec.size() == 0);
    }
}

TEST_CASE("TinyVector resize") {
    SECTION("resize to 0") {
        TinyVector<int> vec;
        vec.push_back(1);
        vec.resize(0);
        REQUIRE(vec.empty());
    }

    SECTION("resize to 1 from empty") {
        TinyVector<int> vec;
        vec.resize(1);
        REQUIRE(vec.size() == 1);
    }

    SECTION("resize to 3 promotes to vector") {
        TinyVector<int> vec;
        vec.resize(3);
        REQUIRE(vec.size() == 3);
        vec[0] = 10;
        vec[1] = 20;
        vec[2] = 30;
        REQUIRE(vec[0] == 10);
        REQUIRE(vec[1] == 20);
        REQUIRE(vec[2] == 30);
    }
}

TEST_CASE("TinyVector at with valid and out-of-range index") {
    TinyVector<int> vec;
    vec.push_back(7);
    vec.push_back(8);

    REQUIRE(vec.at(0) == 7);
    REQUIRE(vec.at(1) == 8);
    REQUIRE_THROWS_AS(vec.at(2), std::out_of_range);
    REQUIRE_THROWS_AS(vec.at(100), std::out_of_range);

    SECTION("at on empty") {
        TinyVector<int> empty_vec;
        REQUIRE_THROWS_AS(empty_vec.at(0), std::out_of_range);
    }

    SECTION("const at") {
        const auto &cvec = vec;
        REQUIRE(cvec.at(0) == 7);
        REQUIRE_THROWS_AS(cvec.at(2), std::out_of_range);
    }
}

TEST_CASE("TinyVector range-based for loop") {
    TinyVector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    int sum = 0;
    for (int val : vec) {
        sum += val;
    }
    REQUIRE(sum == 6);
}

TEST_CASE("TinyVector reserve then push") {
    TinyVector<int> vec;
    vec.reserve(5);

    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == 10);
    REQUIRE(vec[1] == 20);
    REQUIRE(vec[2] == 30);
}

TEST_CASE("TinyVector emplace_back with pair") {
    TinyVector<std::pair<int, int>> vec;

    auto &first = vec.emplace_back(1, 2);
    REQUIRE(first.first == 1);
    REQUIRE(first.second == 2);
    REQUIRE(vec.size() == 1);

    auto &second = vec.emplace_back(3, 4);
    REQUIRE(second.first == 3);
    REQUIRE(second.second == 4);
    REQUIRE(vec.size() == 2);
    REQUIRE(vec[0].first == 1);
    REQUIRE(vec[1].first == 3);
}

TEST_CASE("TinyVector span conversion") {
    TinyVector<int> vec;
    vec.push_back(1);
    vec.push_back(2);

    std::span<int> s = vec;
    REQUIRE(s.size() == 2);
    REQUIRE(s[0] == 1);
    REQUIRE(s[1] == 2);
}
