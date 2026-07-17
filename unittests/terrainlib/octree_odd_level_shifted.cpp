#include <unordered_set>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "octree/Id.h"
#include "octree/OddLevelShifted.h"
#include "octree/Space.h"

using namespace octree;

namespace {

double bounds_epsilon(const Bounds &a, const Bounds &b) {
    const double scale = glm::compMin(glm::min(a.size(), b.size()));
    return std::max(1e-9 * scale, std::numeric_limits<double>::epsilon() * 1000);
}
bool bounds_intersect(const Bounds &a, const Bounds &b, const double epsilon) {
    const glm::dvec3 overlap = glm::min(a.max, b.max) - glm::max(a.min, b.min);
    return glm::all(glm::greaterThan(overlap, glm::dvec3(epsilon)));
}
bool bounds_intersect(const Bounds &a, const Bounds &b) {
    return bounds_intersect(a, b, bounds_epsilon(a, b));
}

std::unordered_set<Id> brute_force_intersecting_nodes_at_level(
    const OddLevelShifted &space,
    const Id &id,
    const Id::Level target_level)
{
    std::unordered_set<Id> result;
    const Bounds id_bounds = space.get_node_bounds(id);
    const Id::Index max_index = Id::max_index_on_level(target_level);

    for (Id::Index index = 0; index <= max_index; index++) {
        const Id candidate(target_level, index);
        if (bounds_intersect(space.get_node_bounds(candidate), id_bounds)) {
            result.insert(candidate);
        }
    }

    return result;
}

} // namespace

TEST_CASE("OddLevelShifted::find_node_at_level_containing_point finds the containing node center", "[octree][odd_level_shifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id expected_id{4, {3, 1, 2}};
    const Bounds bounds = space.get_node_bounds(expected_id);

    const glm::dvec3 point = bounds.centre();

    const auto maybe_id = space.find_node_at_level_containing_point(point, expected_id.level());

    REQUIRE(maybe_id.has_value());
    CHECK(maybe_id.value() == expected_id);
}

TEST_CASE("OddLevelShifted::find_node_at_level_containing_point roundtrips centers for multiple nodes", "[octree][odd_level_shifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const std::vector<Id> ids{
        Id{0, {0, 0, 0}},

        Id{1, {0, 0, 0}},
        Id{1, {1, 0, 0}},
        Id{1, {0, 1, 0}},
        Id{1, {0, 0, 1}},
        Id{1, {1, 1, 1}},

        Id{2, {0, 0, 0}},
        Id{2, {1, 1, 1}},
        Id{2, {2, 1, 3}},
        Id{2, {3, 3, 3}},

        Id{3, {0, 0, 0}},
        Id{3, {3, 1, 2}},
        Id{3, {7, 7, 7}},

        Id{4, {3, 1, 2}},
        Id{4, {4, 0, 0}},
        Id{4, {15, 3, 3}},
        Id{4, {15, 15, 15}},
    };

    for (const Id &expected_id : ids) {
        CAPTURE(expected_id);

        const Bounds bounds = space.get_node_bounds(expected_id);
        const glm::dvec3 point = bounds.centre();

        const auto maybe_id = space.find_node_at_level_containing_point(point, expected_id.level());

        REQUIRE(maybe_id.has_value());
        CHECK(maybe_id.value() == expected_id);
    }
}

TEST_CASE("OddLevelShifted::find_node_at_level_containing_point returns nullopt for points outside the space", "[octree][odd_level_shifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id root_id{0, {0, 0, 0}};
    const Bounds root_bounds = space.get_node_bounds(root_id);

    const glm::dvec3 size = root_bounds.size();

    const std::vector<glm::dvec3> outside_points{
        root_bounds.min - glm::dvec3{1.0, 0.0, 0.0},
        root_bounds.min - glm::dvec3{0.0, 1.0, 0.0},
        root_bounds.min - glm::dvec3{0.0, 0.0, 1.0},

        root_bounds.max + glm::dvec3{1.0, 0.0, 0.0},
        root_bounds.max + glm::dvec3{0.0, 1.0, 0.0},
        root_bounds.max + glm::dvec3{0.0, 0.0, 1.0},

        root_bounds.min - size,
        root_bounds.max + size,
    };

    for (const glm::dvec3 &point : outside_points) {
        CAPTURE(point.x, point.y, point.z);

        const auto maybe_id = space.find_node_at_level_containing_point(point, 4);

        REQUIRE_FALSE(maybe_id.has_value());
    }
}

TEST_CASE("OddLevelShifted::find_node_at_level_containing_point handles lower node boundaries inclusively", "[octree][odd_level_shifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id expected_id{4, {3, 1, 2}};
    const Bounds bounds = space.get_node_bounds(expected_id);

    const glm::dvec3 point = bounds.min;
    const auto maybe_id = space.find_node_at_level_containing_point(point, expected_id.level());

    REQUIRE(maybe_id.has_value());
    CHECK(maybe_id.value() == expected_id);
}

TEST_CASE("OddLevelShifted::find_node_at_level_containing_point handles upper node boundaries exclusively", "[octree][odd_level_shifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id left_id(4, {3, 1, 2});
    const Id right_id(4, {4, 1, 2});

    const Bounds left_bounds = space.get_node_bounds(left_id);

    const glm::dvec3 point_on_shared_x_boundary{
        left_bounds.max.x,
        left_bounds.centre().y,
        left_bounds.centre().z,
    };

    const auto maybe_id = space.find_node_at_level_containing_point(point_on_shared_x_boundary, 4);

    REQUIRE(maybe_id.has_value());
    CHECK(maybe_id.value() == right_id);
}

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_level returns all intersecting nodes at target level", "[octree::OddLevelShifted]") {
    const OddLevelShifted space(Bounds(glm::dvec3(-1), glm::dvec3(1)));
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
    }

    // Test higher level (descendants)
    {
        const Id::Level target_level = id.level() + 1;
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
        const Id::Level target_level = id.level() - 1;
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

TEST_CASE("OddLevelShifted::get_intersecting_nodes_on_level returns all intersecting nodes at levels 0..4",
          "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    constexpr Id::Level max_test_level = 3;

    for (Id::Level source_level = 0; source_level <= max_test_level; source_level++) {
        const Id::Index max_index = Id::max_index_on_level(source_level);
        for (Id::Index i = 0; i <= max_index; i++) {
            const Id id(source_level, i);

            for (Id::Level target_level = 0; target_level <= max_test_level; target_level++) {
                CAPTURE(id, target_level);

                const auto actual_nodes = space.get_intersecting_nodes_on_level(id, target_level);
                const std::unordered_set<Id> actual_set(actual_nodes.begin(), actual_nodes.end());
                const auto expected_set = brute_force_intersecting_nodes_at_level(space, id, target_level);
                const auto [expected_min_it, expected_max_it] = std::ranges::minmax_element(expected_set);
                CHECK(*expected_min_it == actual_nodes.begin_id());
                CHECK(*expected_max_it == actual_nodes.end_id());

                CHECK(actual_set.size() == actual_nodes.size());
                REQUIRE(actual_set == expected_set);
            }
        }
    }
}

TEST_CASE("OddLevelShifted::find_intersecting_nodes_for_standard_id returns nodes covering standard bounds",
          "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    const Space standard_space = Space::earth();

    constexpr Id::Level max_test_level = 3;

    for (Id::Level level = 0; level <= max_test_level; level++) {
        const Id::Index max_index = Id::max_index_on_level(level);
        for (Id::Index i = 0; i <= max_index; i++) {
            const Id id(level, i);
            CAPTURE(id);

            const auto result = space.find_intersecting_nodes_for_standard_id(id);
            const Bounds standard_bounds = standard_space.get_node_bounds(id);

            CHECK_FALSE(result.empty());

            for (const Id &node : result) {
                CHECK(node.level() == level);
                CHECK(bounds_intersect(space.get_node_bounds(node), standard_bounds));
            }
        }
    }
}

TEST_CASE("OddLevelShifted::find_intersecting_nodes_for_standard_id returns self for even levels",
          "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const std::vector<Id> even_ids{
        Id{0, {0, 0, 0}},
        Id{2, {1, 1, 1}},
        Id{2, {3, 3, 3}},
        Id{4, {3, 1, 2}},
    };

    for (const Id &id : even_ids) {
        CAPTURE(id);

        const auto result = space.find_intersecting_nodes_for_standard_id(id);

        CHECK(result.size() == 1);
        CHECK(*result.begin() == id);
    }
}

TEST_CASE("OddLevelShifted::find_intersecting_nodes_for_standard_id returns multiple nodes for odd levels",
          "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id id{3, {3, 3, 3}};
    const auto result = space.find_intersecting_nodes_for_standard_id(id);

    CHECK(result.size() > 1);

    for (const Id &node : result) {
        CHECK(node.level() == id.level());
    }
}

TEST_CASE("OddLevelShifted::get_node_bounds even level equals standard Space bounds", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    const Space standard_space(space.bounds());

    const std::vector<Id> even_ids{
        Id{0, {0, 0, 0}},
        Id{2, {0, 0, 0}},
        Id{2, {1, 2, 3}},
        Id{2, {3, 3, 3}},
        Id{4, {3, 1, 2}},
        Id{4, {15, 15, 15}},
    };

    for (const Id &id : even_ids) {
        CAPTURE(id);

        const Bounds actual = space.get_node_bounds(id);
        const Bounds expected = standard_space.get_node_bounds(id);

        CHECK(actual.min.x == Catch::Approx(expected.min.x));
        CHECK(actual.min.y == Catch::Approx(expected.min.y));
        CHECK(actual.min.z == Catch::Approx(expected.min.z));
        CHECK(actual.max.x == Catch::Approx(expected.max.x));
        CHECK(actual.max.y == Catch::Approx(expected.max.y));
        CHECK(actual.max.z == Catch::Approx(expected.max.z));
    }
}

TEST_CASE("OddLevelShifted::get_node_bounds odd interior node is shifted by half node size", "[octree::OddLevelShifted]") {
    const OddLevelShifted space(Bounds(glm::dvec3(-8), glm::dvec3(8)));
    const Space standard_space(space.bounds());

    const Id id{3, {3, 3, 3}};
    const Bounds standard = standard_space.get_node_bounds(id);
    const Bounds shifted = space.get_node_bounds(id);

    const glm::dvec3 node_size = space.bounds().size() / double(1 << id.level());
    const double half = node_size.x / 2.0;

    CHECK(shifted.min.x == Catch::Approx(standard.min.x - half));
    CHECK(shifted.min.y == Catch::Approx(standard.min.y - half));
    CHECK(shifted.min.z == Catch::Approx(standard.min.z - half));
    CHECK(shifted.max.x == Catch::Approx(standard.max.x - half));
    CHECK(shifted.max.y == Catch::Approx(standard.max.y - half));
    CHECK(shifted.max.z == Catch::Approx(standard.max.z - half));
}

TEST_CASE("OddLevelShifted::get_node_bounds odd border nodes are clipped and extended", "[octree::OddLevelShifted]") {
    const OddLevelShifted space(Bounds(glm::dvec3(-8), glm::dvec3(8)));

    const Id min_border{1, {0, 0, 0}};
    const Bounds min_bounds = space.get_node_bounds(min_border);

    CHECK(min_bounds.min.x == Catch::Approx(space.bounds().min.x));
    CHECK(min_bounds.min.y == Catch::Approx(space.bounds().min.y));
    CHECK(min_bounds.min.z == Catch::Approx(space.bounds().min.z));

    const Id max_border{1, {1, 1, 1}};
    const Bounds max_bounds = space.get_node_bounds(max_border);

    CHECK(max_bounds.max.x == Catch::Approx(space.bounds().max.x));
    CHECK(max_bounds.max.y == Catch::Approx(space.bounds().max.y));
    CHECK(max_bounds.max.z == Catch::Approx(space.bounds().max.z));
}

TEST_CASE("OddLevelShifted::get_node_bounds all level-1 nodes together tile the space", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Bounds min_node = space.get_node_bounds(Id{1, {0, 0, 0}});
    const Bounds max_node = space.get_node_bounds(Id{1, {1, 1, 1}});

    CHECK(min_node.min.x == Catch::Approx(space.bounds().min.x));
    CHECK(min_node.min.y == Catch::Approx(space.bounds().min.y));
    CHECK(min_node.min.z == Catch::Approx(space.bounds().min.z));

    CHECK(max_node.max.x == Catch::Approx(space.bounds().max.x));
    CHECK(max_node.max.y == Catch::Approx(space.bounds().max.y));
    CHECK(max_node.max.z == Catch::Approx(space.bounds().max.z));
}

TEST_CASE("OddLevelShifted::get_node_bounds_with_children non-leaf equals union of node and children bounds", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id id{2, {1, 1, 1}};
    const Bounds actual = space.get_node_bounds_with_children(id);

    Bounds expected = space.get_node_bounds(id);
    expected.expand_by(space.get_node_bounds(id.child(0).value()));
    expected.expand_by(space.get_node_bounds(id.child(7).value()));

    CHECK(actual.min.x == Catch::Approx(expected.min.x));
    CHECK(actual.min.y == Catch::Approx(expected.min.y));
    CHECK(actual.min.z == Catch::Approx(expected.min.z));
    CHECK(actual.max.x == Catch::Approx(expected.max.x));
    CHECK(actual.max.y == Catch::Approx(expected.max.y));
    CHECK(actual.max.z == Catch::Approx(expected.max.z));
}

TEST_CASE("OddLevelShifted::get_node_bounds_with_children even-level includes node and children", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    const Id id{2, {1, 1, 1}};
    const Bounds node = space.get_node_bounds(id);
    const Bounds with_children = space.get_node_bounds_with_children(id);

    CHECK(with_children.min.x < node.min.x);
    CHECK(with_children.min.y < node.min.y);
    CHECK(with_children.min.z < node.min.z);
    CHECK(with_children.max.x == Catch::Approx(node.max.x));
    CHECK(with_children.max.y == Catch::Approx(node.max.y));
    CHECK(with_children.max.z == Catch::Approx(node.max.z));
}

TEST_CASE("OddLevelShifted::contains interior point returns true", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    CHECK(space.contains(space.bounds().centre()));
}

TEST_CASE("OddLevelShifted::contains min corner returns true and max corner returns false", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();

    CHECK(space.contains(space.bounds().min));
    CHECK_FALSE(space.contains(space.bounds().max));
}

TEST_CASE("OddLevelShifted::bounds returns the unshifted original bounds", "[octree::OddLevelShifted]") {
    const OddLevelShifted space = OddLevelShifted::earth();
    const Bounds standard = Space::earth().bounds();

    CHECK(space.bounds().min.x == Catch::Approx(standard.min.x));
    CHECK(space.bounds().min.y == Catch::Approx(standard.min.y));
    CHECK(space.bounds().min.z == Catch::Approx(standard.min.z));
    CHECK(space.bounds().max.x == Catch::Approx(standard.max.x));
    CHECK(space.bounds().max.y == Catch::Approx(standard.max.y));
    CHECK(space.bounds().max.z == Catch::Approx(standard.max.z));
}
