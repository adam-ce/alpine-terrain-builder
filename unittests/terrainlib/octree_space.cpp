#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "octree/Id.h"
#include "octree/Space.h"

using namespace octree;

TEST_CASE("find_smallest_node_encompassing_bounds includes full space", "[octree::Space]") {
    Space space = Space::earth();

    auto full_space_id = space.find_smallest_node_encompassing_bounds(space.bounds());
    REQUIRE(full_space_id.has_value());
    CHECK(full_space_id.value() == Id::root());
}

TEST_CASE("find_smallest_node_encompassing_bounds returns more specific child", "[octree::Space]") {
    Space space = Space::earth();

    Bounds inner_box{
        space.bounds().min,
        space.bounds().min + space.bounds().size() /  2.0
    };

    auto id = space.find_smallest_node_encompassing_bounds(inner_box);
    REQUIRE(id.has_value());
    CHECK(id->level() == 1);
    CHECK(id->coords() == glm::uvec3(0, 0, 0));
}

TEST_CASE("find_smallest_node_encompassing_bounds returns nullopt for out-of-bounds", "[octree::Space]") {
    Space space = Space::earth();
    glm::dvec3 offset = space.bounds().size() * 2.0;

    Bounds far_away{
        space.bounds().min + offset,
        space.bounds().min + offset + glm::dvec3(1.0)
    };

    auto id = space.find_smallest_node_encompassing_bounds(far_away);
    REQUIRE_FALSE(id.has_value());
}

TEST_CASE("find_smallest_node_encompassing_bounds throws on zero-size box", "[octree::Space]") {
    Space space = Space::earth();
    glm::dvec3 p = {1.0, 1.0, 1.0};

    Bounds degenerate{p, p};

    REQUIRE_THROWS_AS(space.find_smallest_node_encompassing_bounds(degenerate), std::invalid_argument);
}

TEST_CASE("find_smallest_node_encompassing_bounds returns smallest valid node", "[octree]") {
    Space space = Space::earth();

    // Pick a very small bounds somewhere inside the root
    glm::dvec3 offset = space.bounds().size() * 0.1;
    glm::dvec3 size = space.bounds().size() * 0.001;

    Bounds target{
        space.bounds().min + offset,
        space.bounds().min + offset + size};

    auto maybe_id = space.find_smallest_node_encompassing_bounds(target);
    REQUIRE(maybe_id.has_value());
    Id id = *maybe_id;

    // Get the bounds of that node
    Bounds node_bounds = space.get_node_bounds(id);

    // Check that node bounds fully contain the target
    for (const auto &corner : radix::geometry::corners(target)) {
        REQUIRE(node_bounds.contains_inclusive(corner));
    }

    // Now check that no child of this node fully contains the target
    for (const auto &child_id : id.children().value()) {
        Bounds child_bounds = space.get_node_bounds(child_id);
        bool all_corners_inside = true;
        for (const auto &corner : radix::geometry::corners(target)) {
            if (!child_bounds.contains_inclusive(corner)) {
                all_corners_inside = false;
                break;
            }
        }
        // At least one child must fail to contain the target bounds
        REQUIRE_FALSE(all_corners_inside);
    }
}

TEST_CASE("find_smallest_node_encompassing_bounds roundtrips from Id to bounds and back", "[octree]") {
    Space space = Space::earth();

    // Pick a non-root ID
    Id original_id{4, {3, 1, 2}}; // Level 4, arbitrary coords

    // Get bounds of this node
    Bounds node_bounds = space.get_node_bounds(original_id);

    // Sanity check: bounds should be non-empty
    auto size = node_bounds.size();
    REQUIRE(size.x > 0);
    REQUIRE(size.y > 0);
    REQUIRE(size.z > 0);

    // Find the smallest node encompassing the bounds
    auto maybe_id = space.find_smallest_node_encompassing_bounds(node_bounds);

    // It should find exactly the same ID
    REQUIRE(maybe_id.has_value());
    REQUIRE(maybe_id.value() == original_id);
}
