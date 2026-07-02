#include "../catch2_helpers.h"
#include "hash_utils.h"

#include <cstdint>
#include <string>

TEST_CASE("default_seed returns expected value", "[hash]") {
    REQUIRE(hash::default_seed() == 0x9e3779b9);
}

TEST_CASE("combine with single int returns non-zero and is deterministic", "[hash]") {
    size_t result1 = hash::combine(42);
    size_t result2 = hash::combine(42);
    CHECK(result1 != 0);
    REQUIRE(result1 == result2);
}

TEST_CASE("combine with two ints is deterministic and differs from single", "[hash]") {
    size_t single = hash::combine(42);
    size_t dual1 = hash::combine(42, 7);
    size_t dual2 = hash::combine(42, 7);
    CHECK(dual1 == dual2);
    REQUIRE(dual1 != single);
}

TEST_CASE("combine is order-dependent", "[hash]") {
    size_t h1 = hash::combine(1, 2);
    size_t h2 = hash::combine(2, 1);
    REQUIRE(h1 != h2);
}

TEST_CASE("combine with same values gives same result", "[hash]") {
    size_t a = hash::combine(10, 20, 30);
    size_t b = hash::combine(10, 20, 30);
    REQUIRE(a == b);
}

TEST_CASE("append modifies seed", "[hash]") {
    size_t seed = hash::default_seed();
    size_t original = seed;
    hash::append(seed, 42);
    REQUIRE(seed != original);
}

TEST_CASE("append with multiple args equivalent to sequential appends", "[hash]") {
    size_t seed_multi = hash::default_seed();
    hash::append(seed_multi, 1, 2, 3);

    size_t seed_seq = hash::default_seed();
    hash::append(seed_seq, 1);
    hash::append(seed_seq, 2);
    hash::append(seed_seq, 3);

    REQUIRE(seed_multi == seed_seq);
}

TEST_CASE("combine with different types", "[hash]") {
    size_t h_int = hash::combine(42);
    size_t h_uint = hash::combine(uint32_t(42));
    size_t h_str = hash::combine(std::string("hello"));

    // Each should produce a valid non-zero hash
    CHECK(h_int != 0);
    CHECK(h_uint != 0);
    CHECK(h_str != 0);

    // String hash should differ from integer hashes
    REQUIRE(h_str != h_int);
}

TEST_CASE("combine produces different hashes for different values", "[hash]") {
    CHECK(hash::combine(0) != hash::combine(1));
    CHECK(hash::combine(1) != hash::combine(2));
    CHECK(hash::combine(100) != hash::combine(200));
    CHECK(hash::combine(1, 2) != hash::combine(3, 4));
}
