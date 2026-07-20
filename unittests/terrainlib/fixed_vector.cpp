#include "../catch2_helpers.h"
#include "FixedVector.h"

TEST_CASE("FixedVector default construction") {
    FixedVector<int, 5> vec;
    CHECK(vec.size() == 0);
    CHECK(vec.capacity() == 5);
    CHECK(vec.empty());
    CHECK(!vec.full());
}

TEST_CASE("FixedVector initializer_list construction") {
    FixedVector<int, 5> vec{1, 2, 3};
    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);
    CHECK(vec[2] == 3);

    REQUIRE_THROWS_AS((FixedVector<int, 2>{1, 2, 3}), std::out_of_range);
}

TEST_CASE("FixedVector copy constructor and assignment") {
    FixedVector<int, 5> vec{1, 2, 3};
    FixedVector<int, 5> copy_vec(vec);
    REQUIRE(copy_vec.size() == vec.size());
    CHECK(copy_vec[1] == 2);

    FixedVector<int, 5> assign_vec;
    assign_vec = vec;
    REQUIRE(assign_vec.size() == vec.size());
    CHECK(assign_vec[2] == 3);

    // Exercise self-assignment through an alias to avoid Clang's syntactic self-assignment warning.
    const auto *self = &vec;
    vec = *self;
    CHECK(vec.size() == 3);
}

TEST_CASE("FixedVector move constructor and assignment") {
    FixedVector<int, 5> vec{1, 2, 3};
    FixedVector<int, 5> moved_vec(std::move(vec));
    CHECK(moved_vec.size() == 3);
    CHECK(vec.size() == 0); // source cleared

    FixedVector<int, 5> another_vec;
    another_vec = std::move(moved_vec);
    CHECK(another_vec.size() == 3);
    CHECK(moved_vec.size() == 0);
}

TEST_CASE("FixedVector emplace_back and push_back") {
    FixedVector<std::pair<int, int>, 3> vec;
    vec.emplace_back(1, 2);
    vec.emplace_back(3, 4);
    REQUIRE(vec.size() == 2);
    CHECK(vec[0].first == 1);
    CHECK(vec[1].second == 4);

    vec.push_back(std::make_pair(5, 6));
    CHECK(vec.size() == 3);
    REQUIRE_THROWS_AS(vec.emplace_back(7, 8), std::out_of_range);
    REQUIRE_THROWS_AS(vec.push_back(std::make_pair(9, 10)), std::out_of_range);
}

TEST_CASE("FixedVector try_emplace_back and try_push_back") {
    FixedVector<int, 2> vec;
    CHECK(vec.try_emplace_back(1));
    CHECK(vec.try_push_back(2));
    REQUIRE_FALSE(vec.try_emplace_back(3));
    REQUIRE_FALSE(vec.try_push_back(4));
}

TEST_CASE("FixedVector pop_back") {
    FixedVector<int, 3> vec{1, 2};
    vec.pop_back();
    REQUIRE(vec.size() == 1);
    CHECK(vec[0] == 1);
    vec.pop_back();
    CHECK(vec.empty());
    REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
}

TEST_CASE("FixedVector at() bounds checking") {
    FixedVector<int, 2> vec{5, 6};
    CHECK(vec.at(0) == 5);
    CHECK(vec.at(1) == 6);
    REQUIRE_THROWS_AS(vec.at(2), std::out_of_range);
}

TEST_CASE("FixedVector clear()") {
    FixedVector<int, 3> vec{1, 2, 3};
    vec.clear();
    CHECK(vec.empty());
    CHECK(vec.size() == 0);
    REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
}

TEST_CASE("FixedVector resize") {
    FixedVector<int, 4> vec{1, 2};
    vec.resize(4);
    REQUIRE(vec.size() == 4);
    vec[2] = 3;
    vec[3] = 4;

    vec.resize(2);
    REQUIRE(vec.size() == 2);
    CHECK(vec[0] == 1);
    CHECK(vec[1] == 2);

    REQUIRE_THROWS_AS(vec.resize(5), std::out_of_range);
}

TEST_CASE("FixedVector begin/end iterators") {
    FixedVector<int, 3> vec{1, 2, 3};
    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
        sum += *it;
    CHECK(sum == 6);

    sum = 0;
    for (int x : vec)
        sum += x;
    CHECK(sum == 6);
}

TEST_CASE("FixedVector std::span conversion") {
    FixedVector<int, 3> vec{1, 2, 3};
    std::span<int> s = vec;
    REQUIRE(s.size() == 3);
    CHECK(s[0] == 1);

    std::span<const int> cs = vec;
    CHECK(cs[1] == 2);
}

TEST_CASE("FixedVector full() and empty()") {
    FixedVector<int, 2> vec;
    CHECK(vec.empty());
    REQUIRE_FALSE(vec.full());

    vec.push_back(1);
    vec.push_back(2);
    CHECK(vec.full());
    REQUIRE_FALSE(vec.empty());
}

TEST_CASE("FixedVector capacity()") {
    FixedVector<int, 4> vec;
    CHECK(vec.capacity() == 4);
    vec.push_back(1);
    vec.push_back(2);
    CHECK(vec.capacity() == 4);
}

TEST_CASE("FixedVector works with std::unique_ptr") {
    FixedVector<std::unique_ptr<int>, 3> vec;

    // emplace_back
    vec.emplace_back(std::make_unique<int>(10));
    vec.emplace_back(std::make_unique<int>(20));
    CHECK(*vec[0] == 10);
    CHECK(*vec[1] == 20);
    CHECK(vec.size() == 2);

    // push_back using move
    auto ptr = std::make_unique<int>(30);
    vec.push_back(std::move(ptr));
    REQUIRE(vec.size() == 3);
    CHECK(*vec[2] == 30);
    CHECK(ptr == nullptr); // moved-from

    // try_emplace_back and capacity check
    REQUIRE_FALSE(vec.try_emplace_back(std::make_unique<int>(40)));
    REQUIRE_THROWS_AS(vec.emplace_back(std::make_unique<int>(50)), std::out_of_range);

    // pop_back
    vec.pop_back();
    REQUIRE(vec.size() == 2);
    CHECK(*vec[0] == 10);
    CHECK(*vec[1] == 20);

    // clear
    vec.clear();
    CHECK(vec.empty());

    // move constructor
    vec.emplace_back(std::make_unique<int>(100));
    vec.emplace_back(std::make_unique<int>(200));
    FixedVector<std::unique_ptr<int>, 3> moved_vec(std::move(vec));
    CHECK(moved_vec.size() == 2);
    CHECK(vec.empty()); // source cleared

    // move assignment
    FixedVector<std::unique_ptr<int>, 3> another_vec;
    another_vec = std::move(moved_vec);
    CHECK(another_vec.size() == 2);
    CHECK(moved_vec.empty());
}

TEST_CASE("FixedVector resize with std::unique_ptr") {
    FixedVector<std::unique_ptr<int>, 4> vec;
    vec.emplace_back(std::make_unique<int>(1));
    vec.emplace_back(std::make_unique<int>(2));

    // Increasing size (default constructs nullptr)
    vec.resize(4);
    REQUIRE(vec.size() == 4);
    CHECK(vec[2] == nullptr);
    CHECK(vec[3] == nullptr);

    // Decreasing size destroys elements
    vec.resize(1);
    REQUIRE(vec.size() == 1);
    CHECK(*vec[0] == 1);

    // Exceed capacity
    REQUIRE_THROWS_AS(vec.resize(5), std::out_of_range);
}
