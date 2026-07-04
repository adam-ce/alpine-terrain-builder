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
        CHECK(node_bounds.contains_inclusive(corner));
    }

    // Now check that no child of this node fully contains the target
    REQUIRE(id.has_children());
    const auto children = id.children().value();
    for (const auto &child_id : children) {
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
    CHECK(size.x > 0);
    CHECK(size.y > 0);
    CHECK(size.z > 0);

    // Find the smallest node encompassing the bounds
    auto maybe_id = space.find_smallest_node_encompassing_bounds(node_bounds);

    // It should find exactly the same ID
    REQUIRE(maybe_id.has_value());
    CHECK(maybe_id.value() == original_id);
}

TEST_CASE("find_node_at_level_containing_point: roundtrip from Id to center back to Id", "[octree::Space]") {
    Space space = Space::earth();

    Id id{4, glm::uvec3(3, 1, 2)};
    glm::dvec3 centre = space.get_node_bounds(id).centre();

    auto result = space.find_node_at_level_containing_point(centre, 4);
    REQUIRE(result.has_value());
    CHECK(result.value() == id);
}

TEST_CASE("find_node_at_level_containing_point: nullopt outside space", "[octree::Space]") {
    Space space = Space::earth();

    glm::dvec3 outside = space.bounds().max + glm::dvec3(1.0);

    auto result = space.find_node_at_level_containing_point(outside, 3);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("find_node_at_level_containing_point: level 0 returns root", "[octree::Space]") {
    Space space = Space::earth();

    glm::dvec3 interior = space.bounds().centre();

    auto result = space.find_node_at_level_containing_point(interior, 0);
    REQUIRE(result.has_value());
    CHECK(result.value() == Id::root());
}

TEST_CASE("find_node_at_level_containing_point: ancestors are consistent across levels", "[octree::Space]") {
    Space space = Space::earth();

    glm::dvec3 point = space.bounds().centre();

    auto id1 = space.find_node_at_level_containing_point(point, 1);
    auto id2 = space.find_node_at_level_containing_point(point, 2);
    auto id3 = space.find_node_at_level_containing_point(point, 3);

    REQUIRE(id1.has_value());
    REQUIRE(id2.has_value());
    REQUIRE(id3.has_value());

    CAPTURE(id1.value());
    CAPTURE(id2.value());
    CAPTURE(id3.value());

    CHECK(id2->ancestor_on_level(1) == id1);
    CHECK(id3->ancestor_on_level(2) == id2);
}

TEST_CASE("get_node_bounds: root equals space bounds", "[octree::Space]") {
    Space space = Space::earth();

    Bounds root_bounds = space.get_node_bounds(Id::root());

    CHECK(root_bounds.min.x == space.bounds().min.x);
    CHECK(root_bounds.min.y == space.bounds().min.y);
    CHECK(root_bounds.min.z == space.bounds().min.z);
    CHECK(root_bounds.max.x == space.bounds().max.x);
    CHECK(root_bounds.max.y == space.bounds().max.y);
    CHECK(root_bounds.max.z == space.bounds().max.z);
}

TEST_CASE("get_node_bounds: children tile parent", "[octree::Space]") {
    Space space = Space::earth();

    Id parent{2, glm::uvec3(1, 2, 0)};
    Bounds parent_bounds = space.get_node_bounds(parent);

    REQUIRE(parent.has_children());
    auto children = parent.children().value();

    Bounds union_bounds = space.get_node_bounds(children[0]);
    for (const Id &child : children) {
        CAPTURE(child);
        union_bounds.expand_by(space.get_node_bounds(child));
    }

    CHECK(union_bounds.min.x == Catch::Approx(parent_bounds.min.x).epsilon(1e-9));
    CHECK(union_bounds.min.y == Catch::Approx(parent_bounds.min.y).epsilon(1e-9));
    CHECK(union_bounds.min.z == Catch::Approx(parent_bounds.min.z).epsilon(1e-9));
    CHECK(union_bounds.max.x == Catch::Approx(parent_bounds.max.x).epsilon(1e-9));
    CHECK(union_bounds.max.y == Catch::Approx(parent_bounds.max.y).epsilon(1e-9));
    CHECK(union_bounds.max.z == Catch::Approx(parent_bounds.max.z).epsilon(1e-9));

    Bounds child0_bounds = space.get_node_bounds(children[0]);
    Bounds child7_bounds = space.get_node_bounds(children[7]);

    CHECK(child0_bounds.min.x == Catch::Approx(parent_bounds.min.x).epsilon(1e-9));
    CHECK(child0_bounds.min.y == Catch::Approx(parent_bounds.min.y).epsilon(1e-9));
    CHECK(child0_bounds.min.z == Catch::Approx(parent_bounds.min.z).epsilon(1e-9));
    CHECK(child7_bounds.max.x == Catch::Approx(parent_bounds.max.x).epsilon(1e-9));
    CHECK(child7_bounds.max.y == Catch::Approx(parent_bounds.max.y).epsilon(1e-9));
    CHECK(child7_bounds.max.z == Catch::Approx(parent_bounds.max.z).epsilon(1e-9));
}

TEST_CASE("get_node_size_at_level: halves at each level", "[octree::Space]") {
    Space space = Space::earth();

    glm::dvec3 full_size = space.bounds().size();

    glm::dvec3 size0 = space.get_node_size_at_level(0);
    glm::dvec3 size1 = space.get_node_size_at_level(1);
    glm::dvec3 size2 = space.get_node_size_at_level(2);

    CHECK(size0.x == Catch::Approx(full_size.x).epsilon(1e-9));
    CHECK(size0.y == Catch::Approx(full_size.y).epsilon(1e-9));
    CHECK(size0.z == Catch::Approx(full_size.z).epsilon(1e-9));

    CHECK(size1.x == Catch::Approx(full_size.x / 2.0).epsilon(1e-9));
    CHECK(size1.y == Catch::Approx(full_size.y / 2.0).epsilon(1e-9));
    CHECK(size1.z == Catch::Approx(full_size.z / 2.0).epsilon(1e-9));

    CHECK(size2.x == Catch::Approx(full_size.x / 4.0).epsilon(1e-9));
    CHECK(size2.y == Catch::Approx(full_size.y / 4.0).epsilon(1e-9));
    CHECK(size2.z == Catch::Approx(full_size.z / 4.0).epsilon(1e-9));
}

TEST_CASE("contains: interior point is true", "[octree::Space]") {
    Space space = Space::earth();

    CHECK(space.contains(space.bounds().centre()));
}

TEST_CASE("contains: min corner is true (inclusive)", "[octree::Space]") {
    Space space = Space::earth();

    CHECK(space.contains(space.bounds().min));
}

TEST_CASE("contains: max corner is false (exclusive)", "[octree::Space]") {
    Space space = Space::earth();

    CHECK_FALSE(space.contains(space.bounds().max));
}
