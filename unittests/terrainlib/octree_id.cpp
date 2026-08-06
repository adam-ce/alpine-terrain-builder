#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "octree/Id.h"

using namespace octree;

using WideId = Id_<3, 64>;   // Index needs wide-integer storage, Coord stays native
using UltraWideId = Id_<3, 200>;  // Index, Coord and SignedCoord all need wide-integer storage

TEMPLATE_TEST_CASE("Id basic construction and accessors", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType id(2, typename TestType::Coords(3, 1, 2));
    CHECK(id.level() == 2);
    CHECK(id.coords() == typename TestType::Coords(3, 1, 2));
    CHECK(id.x() == 3);
    CHECK(id.y() == 1);
    CHECK(id.z() == 2);
}

TEMPLATE_TEST_CASE("Id interleave and deinterleave roundtrip", "[octree::Id]", Id, WideId, UltraWideId) {
    const typename TestType::Coords coords(1, 2, 3);
    TestType id(17, coords);
    CHECK(id.coords() == coords);
}

TEMPLATE_TEST_CASE("Id neighbour within bounds", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType id(3, typename TestType::Coords(4, 4, 4));
    auto neighbor = id.neighbour(typename TestType::Offset(1, -1, 0));
    REQUIRE(neighbor.has_value());
    CHECK(neighbor.value().coords() == typename TestType::Coords(5, 3, 4));
}

TEMPLATE_TEST_CASE("Id neighbour out of bounds returns nullopt", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType id(3, typename TestType::Coords(0, 0, 0));
    CHECK_FALSE(id.neighbour(typename TestType::Offset(-1, 0, 0)).has_value());
    CHECK_FALSE(id.neighbour(typename TestType::Offset(0, -1, 0)).has_value());
    CHECK_FALSE(id.neighbour(typename TestType::Offset(0, 0, -1)).has_value());
    CHECK_FALSE(id.neighbour(typename TestType::Offset(100, 0, 0)).has_value());
    CHECK_FALSE(id.neighbour(typename TestType::Offset(0, TestType::max_coord_on_level(id.level()) + 1, 0)).has_value());
    CHECK_FALSE(id.neighbour(typename TestType::Offset(TestType::max_coord_on_level(id.level()) + 1)).has_value());
}

TEMPLATE_TEST_CASE("Id parent returns nullopt at root level", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType root = TestType::root();
    CHECK_FALSE(root.parent().has_value());
}

TEMPLATE_TEST_CASE("Id parent returns correct parent", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType id(5, typename TestType::Coords(6, 4, 2));
    auto parent = id.parent();
    REQUIRE(parent.has_value());
    CHECK(parent->level() == 4);
    CHECK(parent->coords() == typename TestType::Coords(3, 2, 1));
}

TEMPLATE_TEST_CASE("Id child returns correct child", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType parent(1, typename TestType::Coords(1, 1, 1));
    TestType child = parent.child(5).value();
    CHECK(child.level() == 2);
    CHECK((child.index_on_level() >> 3) == parent.index_on_level());
    CHECK((child.index_on_level() & 7) == 5);
}

TEMPLATE_TEST_CASE("Id child throws on invalid index", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType parent(1, typename TestType::Coords(1, 1, 1));
    CHECK_THROWS_AS(parent.child(8), std::invalid_argument);
    CHECK_THROWS_AS(parent.child(-1), std::invalid_argument);
}

TEMPLATE_TEST_CASE("Id at deep level near storage limits", "[octree::Id]", Id, WideId, UltraWideId) {
    const typename TestType::Coord max_coord = TestType::max_coord_on_level(TestType::max_level());
    const typename TestType::Coords coords(max_coord, max_coord / 2, 0);
    TestType id(TestType::max_level(), coords);
    CHECK(id.coords() == coords);
    CHECK_FALSE(id.has_children());

    auto parent = id.parent();
    REQUIRE(parent.has_value());
    CHECK(parent->coords() == coords / typename TestType::Coords(2));
}

TEMPLATE_TEST_CASE("Id to_string includes level and coords", "[octree::Id]", Id, WideId, UltraWideId) {
    TestType id(2, typename TestType::Coords(3, 1, 2));
    CHECK(id.to_string().find("level=2") != std::string::npos);
}
