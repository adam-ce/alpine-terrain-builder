#include <cstdint>
#include <limits>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "octree/StoreTraits.h"
#include "raster_store/StoreTraits.h"
#include "store/Index.h"

static_assert(store::HierarchyTraits<octree::StoreTraits>);
static_assert(store::HierarchyTraits<raster_store::StoreTraits>);

TEMPLATE_TEST_CASE(
    "shared hierarchy index transitions",
    "[store][index]",
    octree::StoreTraits,
    raster_store::StoreTraits) {
    using Traits = TestType;
    using Index = store::Index<Traits>;

    Index index;
    const auto root = Traits::root();
    const auto children = Traits::children(root).value();
    const auto first = children[0];
    const auto second = children[1];

    REQUIRE(index.add(first).value());
    CHECK(index.is(store::NodeStatus::Virtual, root).value());
    CHECK(index.is(store::NodeStatus::Leaf, first).value());

    REQUIRE(index.add(root).value());
    CHECK(index.is(store::NodeStatus::Inner, root).value());
    CHECK_FALSE(index.add(root).value());

    REQUIRE(index.add(second).value());
    REQUIRE(index.remove(first).value());
    CHECK(index.is(store::NodeStatus::Inner, root).value());
    REQUIRE(index.remove(second).value());
    CHECK(index.is(store::NodeStatus::Leaf, root).value());
    REQUIRE(index.remove(root).value());
    CHECK(index.empty());
}

TEST_CASE("raster store traits validate tile boundaries", "[store][index][raster]") {
    using Traits = raster_store::StoreTraits;
    using Key = Traits::Key;

    CHECK(Traits::is_valid(Key{0, {0, 0}}));
    CHECK_FALSE(Traits::is_valid(Key{0, {1, 0}}));
    CHECK(Traits::is_valid(Key{31, {uint32_t{0x7fffffff}, uint32_t{0x7fffffff}}}));
    CHECK_FALSE(Traits::is_valid(Key{31, {uint32_t{0x80000000}, 0}}));
    CHECK(Traits::is_valid(Key{32, {0, 0}}));
    CHECK(Traits::is_valid(Key{32, {UINT32_MAX, UINT32_MAX}}));
    CHECK_FALSE(Traits::is_valid(Key{33, {0, 0}}));

    CHECK_FALSE(Traits::parent(Traits::root()).has_value());
    CHECK_FALSE(Traits::children(Key{32, {UINT32_MAX, UINT32_MAX}}).has_value());

    const Key level_31{31, {uint32_t{0x7fffffff}, uint32_t{0x7fffffff}}};
    const auto children = Traits::children(level_31);
    REQUIRE(children.has_value());
    CHECK(children->at(0) == Key{32, {UINT32_MAX - 1, UINT32_MAX - 1}});
    CHECK(children->at(1) == Key{32, {UINT32_MAX, UINT32_MAX - 1}});
    CHECK(children->at(2) == Key{32, {UINT32_MAX - 1, UINT32_MAX}});
    CHECK(children->at(3) == Key{32, {UINT32_MAX, UINT32_MAX}});
}

TEST_CASE("shared index rejects invalid keys", "[store][index][raster]") {
    using Traits = raster_store::StoreTraits;
    using Key = Traits::Key;
    store::Index<Traits> index;
    const Key invalid{4, {16, 0}};

    CHECK(index.get(invalid) == std::unexpected(store::InvalidKey<Key>{invalid}));
    CHECK(index.add(invalid) == std::unexpected(store::InvalidKey<Key>{invalid}));
    CHECK(index.remove(invalid) == std::unexpected(store::InvalidKey<Key>{invalid}));
    CHECK(index.is_present(invalid) == std::unexpected(store::InvalidKey<Key>{invalid}));
    CHECK(index.is_absent(invalid) == std::unexpected(store::InvalidKey<Key>{invalid}));
    CHECK(index.is(store::NodeStatus::Leaf, invalid) == std::unexpected(store::InvalidKey<Key>{invalid}));
}
