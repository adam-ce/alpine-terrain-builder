#include "../catch2_helpers.h"
#include "containers/HybridIndexPairMap.h"

#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

using Map = HybridIndexPairMap<uint32_t, uint32_t>;
using Map64 = HybridIndexPairMap<uint32_t, double>;

TEST_CASE("HybridIndexPairMap empty map") {
    Map map;
    CHECK(map.empty());
    CHECK(map.size() == 0);
    CHECK(map.find(0, 0) == std::nullopt);
    CHECK(map.find_ptr(0, 0) == nullptr);
    CHECK_FALSE(map.contains(0, 0));
}

TEST_CASE("HybridIndexPairMap single insert") {
    Map map;
    map.insert_or_assign(3, 7, 42);

    CHECK_FALSE(map.empty());
    CHECK(map.size() == 1);
    CHECK(map.find(3, 7) == 42);
    CHECK(map.contains(3, 7));

    const uint32_t *ptr = map.find_ptr(3, 7);
    REQUIRE(ptr != nullptr);
    CHECK(*ptr == 42);
}

TEST_CASE("HybridIndexPairMap assign over existing key") {
    Map map;
    map.insert_or_assign(5, 10, 100);
    CHECK(map.find(5, 10) == 100);
    CHECK(map.size() == 1);

    map.insert_or_assign(5, 10, 200);
    CHECK(map.find(5, 10) == 200);
    CHECK(map.size() == 1);
}

TEST_CASE("HybridIndexPairMap two different primary keys stay direct") {
    Map map;
    map.insert_or_assign(1, 10, 100);
    map.insert_or_assign(2, 20, 200);

    CHECK(map.size() == 2);
    CHECK(map.find(1, 10) == 100);
    CHECK(map.find(2, 20) == 200);
}

TEST_CASE("HybridIndexPairMap same primary two secondary keys triggers overflow") {
    Map map;
    map.insert_or_assign(1, 10, 100);
    map.insert_or_assign(1, 20, 200);

    CHECK(map.size() == 2);
    CHECK(map.find(1, 10) == 100);
    CHECK(map.find(1, 20) == 200);
}

TEST_CASE("HybridIndexPairMap same primary three secondary keys") {
    Map map;
    map.insert_or_assign(1, 10, 100);
    map.insert_or_assign(1, 20, 200);
    map.insert_or_assign(1, 30, 300);

    CHECK(map.size() == 3);
    CHECK(map.find(1, 10) == 100);
    CHECK(map.find(1, 20) == 200);
    CHECK(map.find(1, 30) == 300);
}

TEST_CASE("HybridIndexPairMap find and contains for non-existent keys") {
    Map map;
    map.insert_or_assign(5, 10, 42);

    // Missing primary key
    CHECK(map.find(99, 10) == std::nullopt);
    CHECK(map.find_ptr(99, 10) == nullptr);
    CHECK_FALSE(map.contains(99, 10));

    // Existing primary, missing secondary
    CHECK(map.find(5, 99) == std::nullopt);
    CHECK(map.find_ptr(5, 99) == nullptr);
    CHECK_FALSE(map.contains(5, 99));

    // After overflow promotion, check missing secondary
    map.insert_or_assign(5, 20, 43);
    CHECK_FALSE(map.contains(5, 99));
    CHECK(map.find(5, 99) == std::nullopt);
}

TEST_CASE("HybridIndexPairMap clear resets everything") {
    Map map;
    map.insert_or_assign(1, 10, 100);
    map.insert_or_assign(2, 20, 200);
    map.insert_or_assign(2, 30, 300);
    CHECK(map.size() == 3);

    map.clear();
    CHECK(map.empty());
    CHECK(map.size() == 0);
    CHECK(map.find(1, 10) == std::nullopt);
    CHECK(map.find(2, 20) == std::nullopt);
    CHECK_FALSE(map.contains(2, 30));

    // Re-insert after clear
    map.insert_or_assign(4, 40, 400);
    CHECK(map.size() == 1);
    CHECK(map.find(4, 40) == 400);
}

TEST_CASE("HybridIndexPairMap entries iteration") {
    Map map;
    map.insert_or_assign(0, 10, 100);
    map.insert_or_assign(1, 20, 200);
    // Trigger overflow on primary key 2
    map.insert_or_assign(2, 30, 300);
    map.insert_or_assign(2, 40, 400);

    using Tuple = std::tuple<uint32_t, uint32_t, uint32_t>;
    std::set<Tuple> collected;
    for (const auto &entry : map.entries()) {
        collected.emplace(entry.primary_key, entry.secondary_key, entry.value);
    }

    CHECK(collected.size() == 4);
    CHECK(collected.count({0, 10, 100}) == 1);
    CHECK(collected.count({1, 20, 200}) == 1);
    CHECK(collected.count({2, 30, 300}) == 1);
    CHECK(collected.count({2, 40, 400}) == 1);
}

TEST_CASE("HybridIndexPairMap with Index == Value (uint32_t, uint32_t)") {
    // This exercises the std::is_same_v<Index, Value> code path
    Map map;
    map.reserve_primary(10);

    map.insert_or_assign(0, 1, 10);
    map.insert_or_assign(0, 2, 20);
    map.insert_or_assign(3, 4, 30);

    CHECK(map.size() == 3);
    CHECK(map.find(0, 1) == 10);
    CHECK(map.find(0, 2) == 20);
    CHECK(map.find(3, 4) == 30);

    // Assign over in overflow
    map.insert_or_assign(0, 1, 99);
    CHECK(map.find(0, 1) == 99);
    CHECK(map.size() == 3);
}

TEST_CASE("HybridIndexPairMap with different Index and Value types") {
    Map64 map;
    map.insert_or_assign(1, 2, 3.14);
    map.insert_or_assign(1, 3, 2.72);

    CHECK(map.size() == 2);
    REQUIRE(map.find(1, 2).has_value());
    CHECK(map.find(1, 2).value() == Catch::Approx(3.14));
    CHECK(map.find(1, 3).value() == Catch::Approx(2.72));
}
