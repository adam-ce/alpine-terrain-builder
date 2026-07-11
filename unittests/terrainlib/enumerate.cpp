#include "../catch2_helpers.h"
#include "enumerate.h"

#include <array>
#include <string>
#include <vector>

TEST_CASE("enumerate empty vector") {
    std::vector<int> vec;
    int count = 0;
    for (auto item : enumerate(vec)) {
        (void)item;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("enumerate vector of ints") {
    std::vector<int> vec = {10, 20, 30};
    std::vector<std::size_t> indices;
    std::vector<int> values;
    for (auto item : enumerate(vec)) {
        indices.push_back(item.index);
        values.push_back(item.value);
    }
    CHECK(indices == std::vector<std::size_t>{0, 1, 2});
    CHECK(values == std::vector<int>{10, 20, 30});
}

TEST_CASE("enumerate with mutation") {
    std::vector<int> vec = {1, 2, 3};
    for (auto item : enumerate(vec)) {
        item.value *= 10;
    }
    CHECK(vec == std::vector<int>{10, 20, 30});
}

TEST_CASE("enumerate const vector") {
    const std::vector<int> vec = {5, 6, 7};
    for (auto item : enumerate(vec)) {
        CHECK(std::is_const_v<std::remove_reference_t<decltype(item.value)>>);
        (void)item;
    }
    // also verify values are correct
    std::vector<int> values;
    for (auto item : enumerate(vec)) {
        values.push_back(item.value);
    }
    CHECK(values == std::vector<int>{5, 6, 7});
}

TEST_CASE("enumerate with custom index type") {
    std::vector<int> vec = {100, 200};
    for (auto item : enumerate<uint32_t>(vec)) {
        CHECK(std::is_same_v<decltype(item.index), uint32_t>);
        (void)item;
    }
}

TEST_CASE("enumerate std::array") {
    std::array<int, 3> arr = {4, 5, 6};
    std::vector<std::size_t> indices;
    std::vector<int> values;
    for (auto item : enumerate(arr)) {
        indices.push_back(item.index);
        values.push_back(item.value);
    }
    CHECK(indices == std::vector<std::size_t>{0, 1, 2});
    CHECK(values == std::vector<int>{4, 5, 6});
}

TEST_CASE("enumerate with structured bindings") {
    std::vector<int> vec = {7, 8, 9};
    std::size_t expected_index = 0;
    for (auto [i, v] : enumerate(vec)) {
        CHECK(i == expected_index);
        CHECK(v == vec[expected_index]);
        ++expected_index;
    }
    CHECK(expected_index == 3);
}

TEST_CASE("enumerate string vector") {
    std::vector<std::string> vec = {"alpha", "beta", "gamma"};
    std::size_t count = 0;
    for (auto item : enumerate(vec)) {
        CHECK(item.index == count);
        CHECK(item.value == vec[count]);
        ++count;
    }
    CHECK(count == 3);
}
