#include "../catch2_helpers.h"
#include "range_utils.h"

#include <string>
#include <vector>

TEST_CASE("contains: found and not found") {
    std::vector<int> v = {1, 2, 3, 4, 5};

    CHECK(contains(v, 3));
    CHECK(contains(v, 1));
    CHECK(contains(v, 5));
    CHECK_FALSE(contains(v, 0));
    CHECK_FALSE(contains(v, 6));
}

TEST_CASE("index_of: found returns index") {
    std::vector<int> v = {10, 20, 30, 40};

    REQUIRE(index_of(v, 10) == 0);
    REQUIRE(index_of(v, 30) == 2);
    REQUIRE(index_of(v, 40) == 3);
}

TEST_CASE("index_of: not found returns nullopt") {
    std::vector<int> v = {10, 20, 30};

    REQUIRE(index_of(v, 99) == std::nullopt);
}

TEST_CASE("index_of: duplicates returns first occurrence") {
    std::vector<int> v = {5, 3, 7, 3, 9};

    REQUIRE(index_of(v, 3) == 1);
}

TEST_CASE("find_ptr: found returns non-null pointer to correct value") {
    std::vector<int> v = {10, 20, 30};

    auto *ptr = find_ptr(v, 20);
    REQUIRE(ptr != nullptr);
    CHECK(*ptr == 20);
    CHECK(ptr == &v[1]);
}

TEST_CASE("find_ptr: not found returns nullptr") {
    std::vector<int> v = {10, 20, 30};

    CHECK(find_ptr(v, 99) == nullptr);
}

TEST_CASE("find: found returns optional with value") {
    std::vector<std::string> v = {"alpha", "beta", "gamma"};

    auto result = find(v, std::string("beta"));
    REQUIRE(result.has_value());
    CHECK(*result == "beta");
}

TEST_CASE("find: not found returns nullopt") {
    std::vector<std::string> v = {"alpha", "beta"};

    CHECK(find(v, std::string("delta")) == std::nullopt);
}

TEST_CASE("find_single: exactly one occurrence returns iterator") {
    std::vector<int> v = {1, 2, 3, 4, 5};

    auto it = find_single(v, 3);
    REQUIRE(it != v.end());
    CHECK(*it == 3);
}

TEST_CASE("find_single: zero occurrences returns end") {
    std::vector<int> v = {1, 2, 3};

    CHECK(find_single(v, 99) == v.end());
}

TEST_CASE("find_single: two occurrences returns end") {
    std::vector<int> v = {1, 2, 3, 2, 5};

    CHECK(find_single(v, 2) == v.end());
}

TEST_CASE("find_single_index: exactly one occurrence returns index") {
    std::vector<int> v = {10, 20, 30, 40};

    auto result = find_single_index(v, 30);
    REQUIRE(result.has_value());
    CHECK(*result == 2);
}

TEST_CASE("find_single_index: zero occurrences returns nullopt") {
    std::vector<int> v = {10, 20, 30};

    CHECK(find_single_index(v, 99) == std::nullopt);
}

TEST_CASE("find_single_index: two occurrences returns nullopt") {
    std::vector<int> v = {10, 20, 10, 30};

    CHECK(find_single_index(v, 10) == std::nullopt);
}

TEST_CASE("transform_vector: transform ints to strings") {
    std::vector<int> v = {1, 2, 3};

    auto result = transform_vector(v, [](int x) { return std::to_string(x); });

    REQUIRE(result.size() == 3);
    CHECK(result[0] == "1");
    CHECK(result[1] == "2");
    CHECK(result[2] == "3");
}

TEST_CASE("transform_vector: transform with identity") {
    std::vector<int> v = {4, 5, 6};

    auto result = transform_vector(v, [](int x) { return x; });

    REQUIRE(result.size() == 3);
    CHECK(result[0] == 4);
    CHECK(result[1] == 5);
    CHECK(result[2] == 6);
}

TEST_CASE("sum with projection: sum of sizes of strings") {
    std::vector<std::string> v = {"ab", "cdef", "g"};

    auto result = sum(v, size_t{0}, [](const std::string &s) { return s.size(); });

    CHECK(result == 7);
}

TEST_CASE("sum direct: sum of ints") {
    std::vector<int> v = {1, 2, 3, 4, 5};

    CHECK(sum(v) == 15);
}

TEST_CASE("range(end): generates [0, end)") {
    auto r = range(5);
    std::vector<int> result(std::ranges::begin(r), std::ranges::end(r));

    REQUIRE(result.size() == 5);
    CHECK(result == std::vector<int>{0, 1, 2, 3, 4});
}

TEST_CASE("range(begin, end): generates [begin, end)") {
    auto r = range(3, 7);
    std::vector<int> result(std::ranges::begin(r), std::ranges::end(r));

    REQUIRE(result.size() == 4);
    CHECK(result == std::vector<int>{3, 4, 5, 6});
}

TEST_CASE("edge case: empty range") {
    std::vector<int> empty;

    CHECK_FALSE(contains(empty, 1));
    CHECK(find(empty, 1) == std::nullopt);
    CHECK(find_ptr(empty, 1) == nullptr);
    CHECK(index_of(empty, 1) == std::nullopt);
    CHECK(find_single(empty, 1) == empty.end());
    CHECK(find_single_index(empty, 1) == std::nullopt);
    CHECK(sum(empty) == 0);
}
