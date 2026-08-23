#include <catch2/catch_test_macros.hpp>

#include "octree/StoreTraits.h"
#include "sf/validate_index.h"

TEST_CASE("Structura Fundamentalis validation accepts Leaf and Virtual nodes", "[sf][store]")
{
    store::Index<octree::StoreTraits> index;
    const octree::Id leaf = octree::Id::root().child(2).value().child(5).value();
    REQUIRE(index.add(leaf).has_value());

    CHECK(sf::validate_index(index).has_value());
}

TEST_CASE("Structura Fundamentalis validation reports the first Inner node", "[sf][store]")
{
    store::Index<octree::StoreTraits> index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(3).value();
    REQUIRE(index.add(root).has_value());
    REQUIRE(index.add(child).has_value());

    const auto result = sf::validate_index(index);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().key == root);
}
