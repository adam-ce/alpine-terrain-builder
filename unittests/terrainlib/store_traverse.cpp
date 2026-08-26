#include <vector>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "octree/StoreTraits.h"
#include "raster_store/StoreTraits.h"
#include "store/traverse.h"

TEMPLATE_TEST_CASE("shared hierarchy traversal preserves trait child order", "[store][traverse]", octree::StoreTraits, raster_store::StoreTraits)
{
    using Traits = TestType;
    using Key = typename Traits::Key;

    store::Index<Traits> index;
    const Key root = Traits::root();
    const auto children = Traits::children(root).value();
    const Key first = children[0];
    const Key second = children[1];
    const Key grandchild = Traits::children(first).value()[0];
    REQUIRE(index.add(root).has_value());
    REQUIRE(index.add(first).has_value());
    REQUIRE(index.add(second).has_value());
    REQUIRE(index.add(grandchild).has_value());

    std::vector<Key> depth_first;
    REQUIRE(store::traverse(index, [&](const Key& key, const store::NodeStatus) { depth_first.push_back(key); }).has_value());
    CHECK(depth_first == std::vector<Key> { root, first, grandchild, second });

    std::vector<Key> breadth_first;
    REQUIRE(store::traverse(
        index,
        [&](const Key& key, const store::NodeStatus) { breadth_first.push_back(key); },
        store::AlwaysRefine {},
        root,
        store::TraversalOrder::BreadthFirst)
            .has_value());
    CHECK(breadth_first == std::vector<Key> { root, first, second, grandchild });

    std::vector<Key> explicit_root;
    REQUIRE(store::traverse(index, [&](const Key& key, const store::NodeStatus) { explicit_root.push_back(key); }, store::AlwaysRefine {}, first).has_value());
    CHECK(explicit_root == std::vector<Key> { first, grandchild });
}

TEST_CASE("shared traversal rejects an invalid explicit root", "[store][traverse][raster]")
{
    using Traits = raster_store::StoreTraits;
    using Key = Traits::Key;
    const Key invalid { 1, { 2, 0 } };
    store::Index<Traits> index;

    const auto result = store::traverse(index, [](const Key&, const store::NodeStatus) { }, store::AlwaysRefine {}, invalid);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == ::Error::Code::InvalidInput);
}
