#include <unordered_set>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "octree/Id.h"
#include "octree/OddLevelShifted.h"

using namespace octree;

namespace {

bool bounds_intersect_exclusive(const Bounds &a, const Bounds &b) {
    return a.min.x < b.max.x && b.min.x < a.max.x
        && a.min.y < b.max.y && b.min.y < a.max.y
        && a.min.z < b.max.z && b.min.z < a.max.z;
}

std::unordered_set<Id> brute_force_intersecting_nodes_at_level(
    const OddLevelShifted &space,
    const Id &id,
    const uint32_t target_level)
{
    std::unordered_set<Id> result;
    const Bounds id_bounds = space.get_node_bounds(id);
    const Id::Index max_index = Id::max_index_on_level(target_level);

    for (Id::Index index = 0; index <= max_index; index++) {
        const Id candidate(target_level, index);
        if (bounds_intersect_exclusive(space.get_node_bounds(candidate), id_bounds)) {
            result.insert(candidate);
        }
    }

    return result;
}

} // namespace

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_next_level(odd level)", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    
    for (Id::Index index = 0; index < 8; index++) {
        const Id parent_id(1, index);

        const auto actual_nodes = space.get_intersecting_nodes_on_next_level(parent_id);
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, parent_id, parent_id.level() + 1);

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);

        for (const auto &child_id : actual_nodes) {
            CHECK(child_id.level() == parent_id.level() + 1);
        }
    }
}

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_next_level(even level)", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    
    for (Id::Index index = 0; index < 8; index++) {
        const Id parent_id(2, index);

        const auto actual_nodes = space.get_intersecting_nodes_on_next_level(parent_id);
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, parent_id, parent_id.level() + 1);

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);

        for (const auto &child_id : actual_nodes) {
            CHECK(child_id.level() == parent_id.level() + 1);
        }
    }
}

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_previous_level(odd level)", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    
    for (Id::Index index = 0; index < 8; index++) {
        const Id child_id(3, index);

        const auto actual_nodes = space.get_intersecting_nodes_on_previous_level(child_id);
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, child_id, child_id.level() - 1);

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);

        for (const auto &parent_id : actual_nodes) {
            CHECK(parent_id.level() == child_id.level() - 1);
        }
    }
}

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_previous_level(even level)", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    
    for (Id::Index index = 0; index < 8; index++) {
        const Id child_id(2, index);

        const auto actual_nodes = space.get_intersecting_nodes_on_previous_level(child_id);
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, child_id, child_id.level() - 1);

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);

        for (const auto &parent_id : actual_nodes) {
            CHECK(parent_id.level() == child_id.level() - 1);
        }
    }
}

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_level returns all intersecting nodes at target level", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    const Id id(2, glm::uvec3(1, 1, 1));

    // Test same level
    {
        const auto actual_nodes = space.get_intersecting_nodes_on_level(id, id.level());
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, id, id.level());

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);
        CHECK(actual_nodes.size() == 1);
        CHECK(actual_nodes[0] == id);
    }

    // Test higher level (descendants)
    {
        const uint32_t target_level = id.level() + 2;
        const auto actual_nodes = space.get_intersecting_nodes_on_level(id, target_level);
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, id, target_level);

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);

        for (const auto &node : actual_nodes) {
            CHECK(node.level() == target_level);
        }
    }

    // Test lower level (ancestors)
    {
        const uint32_t target_level = id.level() - 1;
        const auto actual_nodes = space.get_intersecting_nodes_on_level(id, target_level);
        const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
        const auto expected_set = brute_force_intersecting_nodes_at_level(space, id, target_level);

        REQUIRE_FALSE(actual_nodes.empty());
        CHECK(actual_set.size() == actual_nodes.size());
        CHECK(actual_set == expected_set);

        for (const auto &node : actual_nodes) {
            CHECK(node.level() == target_level);
        }
    }
}
