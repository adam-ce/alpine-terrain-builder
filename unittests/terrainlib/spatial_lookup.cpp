#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

#include "../catch2_helpers.h"
#include "spatial_lookup/Grid.h"
#include "spatial_lookup/Hashmap.h"

// 3D string test
TEMPLATE_TEST_CASE("SpatialLookup insert and find_near", "[SpatialLookup]",
                   (spatial_lookup::Hashmap<3, float, std::string>),
                   (spatial_lookup::Grid<3, float, std::string>)) {
    std::optional<TestType> map_opt = std::nullopt;
    if constexpr (std::is_same_v<TestType, spatial_lookup::Hashmap<3, float, std::string>>) {
        map_opt = spatial_lookup::Hashmap<3, float, std::string>(0.1f);
    } else {
        map_opt = spatial_lookup::Grid<3, float, std::string>(glm::vec3(0.0f), glm::vec3(4.0f), glm::uvec3(4));
    }
    TestType map = std::move(map_opt.value());

    const glm::vec3 p1(1.0, 2.0, 3.0);
    const glm::vec3 p2(1.05, 2.0, 3.0); // within epsilon of p1
    const glm::vec3 p3(2.0, 2.0, 3.0);  // outside epsilon of p1

    map.insert(p1, "P1");
    map.insert(p2, "P2");
    map.insert(p3, "P3");

    std::vector<std::string> found;

    SECTION("Find points near p2 within 0.01") {
        const bool found_any = map.find_all_near(p2, 0.01, found);
        CHECK(found_any);
        CHECK_THAT(found, Catch::Matchers::UnorderedEquals<std::string>({"P2"}));
    }

    SECTION("Find points near p1 within 0.1") {
        const bool found_any = map.find_all_near(p1, 0.1, found);
        CHECK(found_any);
        CHECK_THAT(found, Catch::Matchers::UnorderedEquals<std::string>({"P1", "P2"}));
    }

    SECTION("Find points near p1 with tighter epsilon") {
        const bool found_any = map.find_all_near(p1, 0.04, found);
        CHECK(found_any);
        CHECK_THAT(found, Catch::Matchers::UnorderedEquals<std::string>({"P1"}));
    }

    SECTION("Find points not near any other points") {
        const bool found_any = map.find_all_near(glm::vec3(3.0, 3.0, 3.0), 1, found);
        CHECK(!found_any);
        CHECK_THAT(found, Catch::Matchers::UnorderedEquals<std::string>({}));
    }
}

// 2D int test
TEMPLATE_TEST_CASE("SpatialLookup 2D and int keys", "[SpatialLookup]",
                   (spatial_lookup::Hashmap<2, int, int>),
                   (spatial_lookup::Grid<2, int, int>)) {
    std::optional<TestType> map_opt = std::nullopt;
    if constexpr (std::is_same_v<TestType, spatial_lookup::Hashmap<2, int, int>>) {
        map_opt = spatial_lookup::Hashmap<2, int, int>(1);
    } else {
        map_opt = spatial_lookup::Grid<2, int, int>(glm::ivec2(0), glm::ivec2(6), glm::uvec2(6));
    }
    TestType map = std::move(map_opt.value());

    const glm::ivec2 p1(0, 0);
    const glm::ivec2 p2(1, 1);
    const glm::ivec2 p3(5, 5);

    map.insert(p1, 100);
    map.insert(p2, 200);
    map.insert(p3, 300);

    std::vector<int> found;

    SECTION("Find near p1 with epsilon 1") {
        const bool ok = map.find_all_near(p1, 1, found);
        CHECK(ok);
        CHECK_THAT(found, Catch::Matchers::UnorderedEquals<int>({100}));
    }

    SECTION("Find near p2 with epsilon 2") {
        const bool ok = map.find_all_near(p2, 2, found);
        CHECK(ok);
        CHECK_THAT(found, Catch::Matchers::UnorderedEquals<int>({100, 200}));
    }

    SECTION("Find away with epsilon 1 fails") {
        const bool ok = map.find_all_near(glm::ivec2(3, 3), 1, found);
        CHECK(!ok);
        CHECK(found.empty());
    }
}

// 3D int duplicate points test
TEMPLATE_TEST_CASE("SpatialLookup with duplicate points", "[SpatialLookup]",
                   (spatial_lookup::Hashmap<3, float, int>),
                   (spatial_lookup::Grid<3, float, int>)) {
    std::optional<TestType> map_opt = std::nullopt;
    if constexpr (std::is_same_v<TestType, spatial_lookup::Hashmap<3, float, int>>) {
        map_opt = spatial_lookup::Hashmap<3, float, int>(0.1f);
    } else {
        map_opt = spatial_lookup::Grid<3, float, int>(glm::vec3(0.0f), glm::vec3(2.0f), glm::uvec3(2));
    }
    TestType map = std::move(map_opt.value());

    const glm::vec3 p(1.0f, 1.0f, 1.0f);
    map.insert(p, 42);
    map.insert(p, 43);

    std::vector<int> found;
    const bool ok = map.find_all_near(p, 0.01f, found);

    CHECK(ok);
    CHECK_THAT(found, Catch::Matchers::UnorderedEquals<int>({42, 43}));
}

// Epsilon and vertex position of a level 14 node of the stephansdom dataset.
constexpr double earth_centered_epsilon = 0.25718408203125;
constexpr glm::dvec3 earth_centered_position(4085801.6236, 1199335.1025, 4732875.8476);

// A cell is identified by its quantized corner, which the hash quantizes a second time.
// At earth-centered magnitudes that round trip can land a whole cell lower, so the lookup
// scans the neighbouring cell and never sees the point it started from.
TEST_CASE("SpatialLookup finds a point at earth-centered magnitudes", "[SpatialLookup]") {
    spatial_lookup::Hashmap3d<int> map(earth_centered_epsilon * 5);
    map.insert(earth_centered_position, 42);

    std::vector<int> found;
    const bool ok = map.find_all_near(earth_centered_position, earth_centered_epsilon, found);

    CHECK(ok);
    CHECK_THAT(found, Catch::Matchers::UnorderedEquals<int>({42}));
}

TEST_CASE("SpatialLookup finds every stored point at earth-centered magnitudes", "[SpatialLookup]") {
    constexpr uint32_t point_count = 1000;

    spatial_lookup::Hashmap3d<uint32_t> map(earth_centered_epsilon * 5);
    std::vector<glm::dvec3> positions;
    for (uint32_t i = 0; i < point_count; i++) {
        const glm::dvec3 position = earth_centered_position + glm::dvec3(i * 0.7, i * 1.3, i * 2.1);
        positions.push_back(position);
        map.insert(position, i);
    }

    uint32_t lost = 0;
    std::vector<uint32_t> found;
    for (uint32_t i = 0; i < point_count; i++) {
        map.find_all_near(positions[i], earth_centered_epsilon, found);
        if (std::ranges::find(found, i) == found.end()) {
            lost++;
        }
    }

    CHECK(lost == 0);
}

// Two points at the exact same spot must always end up in the same cell.
TEST_CASE("SpatialLookup finds coincident points at earth-centered magnitudes", "[SpatialLookup]") {
    spatial_lookup::Hashmap3d<int> map(earth_centered_epsilon * 5);
    map.insert(earth_centered_position, 42);
    map.insert(earth_centered_position, 43);

    std::vector<int> found;
    const bool ok = map.find_all_near(earth_centered_position, earth_centered_epsilon, found);

    CHECK(ok);
    CHECK_THAT(found, Catch::Matchers::UnorderedEquals<int>({42, 43}));
}
