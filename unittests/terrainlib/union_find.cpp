#include "../catch2_helpers.h"
#include "containers/UnionFind.h"

TEST_CASE("UnionFind empty (size 0)", "[UnionFind]") {
    UnionFind uf(0);
    CHECK(uf.size() == 0);
    CHECK(uf.set_count() == 0);
}

TEST_CASE("UnionFind single element", "[UnionFind]") {
    UnionFind uf(1);
    CHECK(uf.size() == 1);
    CHECK(uf.set_count() == 1);
    CHECK(uf.find(0) == 0);
    CHECK(uf.is_joint());
}

TEST_CASE("UnionFind no unions", "[UnionFind]") {
    UnionFind uf(5);
    CHECK(uf.size() == 5);
    CHECK(uf.set_count() == 5);
    CHECK(uf.is_disjoint());
    for (size_t i = 0; i < 5; i++) {
        CHECK(uf.find(i) == i);
    }
}

TEST_CASE("UnionFind union two elements", "[UnionFind]") {
    UnionFind uf(4);
    uf.make_union(1, 3);
    CHECK(uf.set_count() == 3);
    CHECK(uf.find(1) == uf.find(3));
    // Elements 0 and 2 remain in their own sets
    REQUIRE(uf.find(0) != uf.find(1));
    REQUIRE(uf.find(2) != uf.find(1));
}

TEST_CASE("UnionFind union chain", "[UnionFind]") {
    UnionFind uf(5);
    uf.make_union(0, 1);
    uf.make_union(1, 2);
    CHECK(uf.find(0) == uf.find(1));
    CHECK(uf.find(1) == uf.find(2));
    CHECK(uf.set_count() == 3);
}

TEST_CASE("UnionFind union already same set", "[UnionFind]") {
    UnionFind uf(3);
    uf.make_union(0, 1);
    CHECK(uf.set_count() == 2);
    uf.make_union(0, 1);
    CHECK(uf.set_count() == 2);
    uf.make_union(1, 0);
    CHECK(uf.set_count() == 2);
}

TEST_CASE("UnionFind get_sets", "[UnionFind]") {
    UnionFind uf(5);
    uf.make_union(0, 2);
    uf.make_union(1, 3);

    auto sets = uf.get_sets_as_map();
    REQUIRE(sets.size() == 3);

    // Each set should contain the right elements
    auto& set_with_0 = sets.at(uf.find(0));
    auto& set_with_1 = sets.at(uf.find(1));
    auto& set_with_4 = sets.at(uf.find(4));

    CHECK(set_with_0.size() == 2);
    CHECK(set_with_1.size() == 2);
    CHECK(set_with_4.size() == 1);
}

TEST_CASE("UnionFind reset restores individual sets", "[UnionFind]") {
    UnionFind uf(4);
    uf.make_union(0, 1);
    uf.make_union(2, 3);
    CHECK(uf.set_count() == 2);

    uf.reset();
    CHECK(uf.size() == 4);
    CHECK(uf.set_count() == 4);
    for (size_t i = 0; i < 4; i++) {
        CHECK(uf.find(i) == i);
    }
}

TEST_CASE("UnionFind reset with new size", "[UnionFind]") {
    UnionFind uf(3);
    uf.make_union(0, 1);
    CHECK(uf.size() == 3);

    uf.reset(6);
    CHECK(uf.size() == 6);
    CHECK(uf.set_count() == 6);
    for (size_t i = 0; i < 6; i++) {
        CHECK(uf.find(i) == i);
    }
}

TEST_CASE("UnionFindWithSizes get_set_size", "[UnionFind]") {
    UnionFindWithSizes uf(5);
    for (size_t i = 0; i < 5; i++) {
        CHECK(uf.get_set_size(i) == 1);
    }

    uf.make_union(0, 1);
    CHECK(uf.get_set_size(0) == 2);
    CHECK(uf.get_set_size(1) == 2);

    uf.make_union(2, 3);
    uf.make_union(0, 2);
    CHECK(uf.get_set_size(0) == 4);
    CHECK(uf.get_set_size(3) == 4);

    // Element 4 still alone
    CHECK(uf.get_set_size(4) == 1);
}

TEST_CASE("UnionFind is_joint after unioning all", "[UnionFind]") {
    UnionFind uf(4);
    CHECK(uf.is_disjoint());

    uf.make_union(0, 1);
    uf.make_union(2, 3);
    CHECK(uf.is_disjoint());

    uf.make_union(0, 2);
    CHECK(uf.is_joint());
    CHECK(uf.set_count() == 1);
}

TEST_CASE("UnionFind path compression consistency", "[UnionFind]") {
    UnionFind uf(6);
    uf.make_union(0, 1);
    uf.make_union(1, 2);
    uf.make_union(2, 3);

    // const find (no path compression)
    const auto& cuf = uf;
    const size_t rep_const = cuf.find(0);

    // non-const find (with path compression)
    const size_t rep_mutable = uf.find(0);

    CHECK(rep_const == rep_mutable);

    // After path compression, all elements should still resolve to the same rep
    CHECK(uf.find(0) == uf.find(3));
    CHECK(uf.find(1) == uf.find(2));
    CHECK(uf.find(0) == uf.find(1));
}
