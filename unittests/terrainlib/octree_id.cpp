#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "octree/Id.h"

using namespace octree;

TEST_CASE("Id basic construction and accessors", "[octree::Id]") {
    Id id(2, glm::uvec3(3, 1, 2));
    CHECK(id.level() == 2);
    CHECK(id.coords() == glm::uvec3(3, 1, 2));
    CHECK(id.x() == 3);
    CHECK(id.y() == 1);
    CHECK(id.z() == 2);
}

TEST_CASE("Id interleave and deinterleave roundtrip", "[octree::Id]") {
    glm::uvec3 coords(1, 2, 3);
    Id id(17, coords);
    CHECK(id.coords() == coords);
}

TEST_CASE("Id neighbour within bounds", "[octree::Id]") {
    Id id(3, glm::uvec3(4, 4, 4));
    auto neighbor = id.neighbour(glm::ivec3(1, -1, 0));
    REQUIRE(neighbor.has_value());
    CHECK(neighbor.value().coords() == glm::uvec3(5, 3, 4));
}

TEST_CASE("Id neighbour out of bounds returns nullopt", "[octree::Id]") {
    Id id(3, glm::uvec3(0, 0, 0));
    CHECK_FALSE(id.neighbour(glm::ivec3(-1, 0, 0)).has_value());
    CHECK_FALSE(id.neighbour(glm::ivec3(0, -1, 0)).has_value());
    CHECK_FALSE(id.neighbour(glm::ivec3(0, 0, -1)).has_value());
    CHECK_FALSE(id.neighbour(glm::ivec3(100, 0, 0)).has_value());
    CHECK_FALSE(id.neighbour(glm::ivec3(0, Id::max_coord_on_level(id.level()) + 1, 0)).has_value());
    CHECK_FALSE(id.neighbour(glm::ivec3(Id::max_coord_on_level(id.level()) + 1)).has_value());
}

TEST_CASE("Id parent returns nullopt at root level", "[octree::Id]") {
    Id root = Id::root();
    CHECK_FALSE(root.parent().has_value());
}

TEST_CASE("Id parent returns correct parent", "[octree::Id]") {
    Id id(5, glm::uvec3(6, 4, 2));
    auto parent = id.parent();
    REQUIRE(parent.has_value());
    CHECK(parent->level() == 4);
    CHECK(parent->coords() == glm::uvec3(3, 2, 1));
}

TEST_CASE("Id child returns correct child", "[octree::Id]") {
    Id parent(1, glm::uvec3(1, 1, 1));
    Id child = parent.child(5).value();
    CHECK(child.level() == 2);
    CHECK((child.index_on_level() >> 3) == parent.index_on_level());
    CHECK((child.index_on_level() & 7) == 5);
}

TEST_CASE("Id child throws on invalid index", "[octree::Id]") {
    Id parent(1, glm::uvec3(1, 1, 1));
    CHECK_THROWS_AS(parent.child(8), std::invalid_argument);
    CHECK_THROWS_AS(parent.child(-1), std::invalid_argument);
}