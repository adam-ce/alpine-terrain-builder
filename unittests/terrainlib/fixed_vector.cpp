#include "../catch2_helpers.h"
#include "FixedVector.h"

TEST_CASE("FixedVector default construction") {
    FixedVector<int, 5> vec;
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.capacity() == 5);
    REQUIRE(vec.empty());
    REQUIRE(!vec.full());
}

TEST_CASE("FixedVector initializer_list construction") {
    FixedVector<int, 5> vec{1, 2, 3};
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);

    REQUIRE_THROWS_AS((FixedVector<int, 2>{1, 2, 3}), std::out_of_range);
}

TEST_CASE("FixedVector copy constructor and assignment") {
    FixedVector<int, 5> vec{1, 2, 3};
    FixedVector<int, 5> copy_vec(vec);
    REQUIRE(copy_vec.size() == vec.size());
    REQUIRE(copy_vec[1] == 2);

    FixedVector<int, 5> assign_vec;
    assign_vec = vec;
    REQUIRE(assign_vec.size() == vec.size());
    REQUIRE(assign_vec[2] == 3);

    // Self-assignment
    vec = vec;
    REQUIRE(vec.size() == 3);
}

TEST_CASE("FixedVector move constructor and assignment") {
    FixedVector<int, 5> vec{1, 2, 3};
    FixedVector<int, 5> moved_vec(std::move(vec));
    REQUIRE(moved_vec.size() == 3);
    REQUIRE(vec.size() == 0); // source cleared

    FixedVector<int, 5> another_vec;
    another_vec = std::move(moved_vec);
    REQUIRE(another_vec.size() == 3);
    REQUIRE(moved_vec.size() == 0);
}

TEST_CASE("FixedVector emplace_back and push_back") {
    FixedVector<std::pair<int, int>, 3> vec;
    vec.emplace_back(1, 2);
    vec.emplace_back(3, 4);
    REQUIRE(vec.size() == 2);
    REQUIRE(vec[0].first == 1);
    REQUIRE(vec[1].second == 4);

    vec.push_back(std::make_pair(5, 6));
    REQUIRE(vec.size() == 3);
    REQUIRE_THROWS_AS(vec.emplace_back(7, 8), std::out_of_range);
    REQUIRE_THROWS_AS(vec.push_back(std::make_pair(9, 10)), std::out_of_range);
}

TEST_CASE("FixedVector try_emplace_back and try_push_back") {
    FixedVector<int, 2> vec;
    REQUIRE(vec.try_emplace_back(1));
    REQUIRE(vec.try_push_back(2));
    REQUIRE_FALSE(vec.try_emplace_back(3));
    REQUIRE_FALSE(vec.try_push_back(4));
}

TEST_CASE("FixedVector pop_back") {
    FixedVector<int, 3> vec{1, 2};
    vec.pop_back();
    REQUIRE(vec.size() == 1);
    REQUIRE(vec[0] == 1);
    vec.pop_back();
    REQUIRE(vec.empty());
    REQUIRE_THROWS_AS(vec.pop_back(), std::out_of_range);
}

TEST_CASE("FixedVector at() bounds checking") {
    FixedVector<int, 2> vec{5, 6};
    REQUIRE(vec.at(0) == 5);
    REQUIRE(vec.at(1) == 6);
    REQUIRE_THROWS_AS(vec.at(2), std::out_of_range);
}

TEST_CASE("FixedVector clear()") {
    FixedVector<int, 3> vec{1, 2, 3};
    vec.clear();
    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
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
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);

    REQUIRE_THROWS_AS(vec.resize(5), std::out_of_range);
}

TEST_CASE("FixedVector begin/end iterators") {
    FixedVector<int, 3> vec{1, 2, 3};
    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
        sum += *it;
    REQUIRE(sum == 6);

    sum = 0;
    for (int x : vec)
        sum += x;
    REQUIRE(sum == 6);
}

TEST_CASE("FixedVector std::span conversion") {
    FixedVector<int, 3> vec{1, 2, 3};
    std::span<int> s = vec;
    REQUIRE(s.size() == 3);
    REQUIRE(s[0] == 1);

    std::span<const int> cs = vec;
    REQUIRE(cs[1] == 2);
}

TEST_CASE("FixedVector full() and empty()") {
    FixedVector<int, 2> vec;
    REQUIRE(vec.empty());
    REQUIRE_FALSE(vec.full());

    vec.push_back(1);
    vec.push_back(2);
    REQUIRE(vec.full());
    REQUIRE_FALSE(vec.empty());
}

TEST_CASE("FixedVector capacity()") {
    FixedVector<int, 4> vec;
    REQUIRE(vec.capacity() == 4);
    vec.push_back(1);
    vec.push_back(2);
    REQUIRE(vec.capacity() == 4);
}

TEST_CASE("FixedVector works with std::unique_ptr") {
    FixedVector<std::unique_ptr<int>, 3> vec;

    // emplace_back
    vec.emplace_back(std::make_unique<int>(10));
    vec.emplace_back(std::make_unique<int>(20));
    REQUIRE(*vec[0] == 10);
    REQUIRE(*vec[1] == 20);
    REQUIRE(vec.size() == 2);

    // push_back using move
    auto ptr = std::make_unique<int>(30);
    vec.push_back(std::move(ptr));
    REQUIRE(vec.size() == 3);
    REQUIRE(*vec[2] == 30);
    REQUIRE(ptr == nullptr); // moved-from

    // try_emplace_back and capacity check
    REQUIRE_FALSE(vec.try_emplace_back(std::make_unique<int>(40)));
    REQUIRE_THROWS_AS(vec.emplace_back(std::make_unique<int>(50)), std::out_of_range);

    // pop_back
    vec.pop_back();
    REQUIRE(vec.size() == 2);
    REQUIRE(*vec[0] == 10);
    REQUIRE(*vec[1] == 20);

    // clear
    vec.clear();
    REQUIRE(vec.empty());

    // move constructor
    vec.emplace_back(std::make_unique<int>(100));
    vec.emplace_back(std::make_unique<int>(200));
    FixedVector<std::unique_ptr<int>, 3> moved_vec(std::move(vec));
    REQUIRE(moved_vec.size() == 2);
    REQUIRE(vec.empty()); // source cleared

    // move assignment
    FixedVector<std::unique_ptr<int>, 3> another_vec;
    another_vec = std::move(moved_vec);
    REQUIRE(another_vec.size() == 2);
    REQUIRE(moved_vec.empty());
}

TEST_CASE("FixedVector resize with std::unique_ptr") {
    FixedVector<std::unique_ptr<int>, 4> vec;
    vec.emplace_back(std::make_unique<int>(1));
    vec.emplace_back(std::make_unique<int>(2));

    // Increasing size (default constructs nullptr)
    vec.resize(4);
    REQUIRE(vec.size() == 4);
    REQUIRE(vec[2] == nullptr);
    REQUIRE(vec[3] == nullptr);

    // Decreasing size destroys elements
    vec.resize(1);
    REQUIRE(vec.size() == 1);
    REQUIRE(*vec[0] == 1);

    // Exceed capacity
    REQUIRE_THROWS_AS(vec.resize(5), std::out_of_range);
}
