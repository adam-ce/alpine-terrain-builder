#include <atomic>
#include <chrono>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "octree/storage/open.h"
#include "terrainbuilder.h"

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path()
            / ("atb-sfbuilder-finalize-" + std::to_string(timestamp) + "-"
               + std::to_string(counter++));
        REQUIRE(std::filesystem::create_directories(_path));
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    const std::filesystem::path &path() const {
        return _path;
    }

private:
    std::filesystem::path _path;
};

mesh::Simple fixture_mesh() {
    const std::filesystem::path fixture_path =
        std::filesystem::path(ALP_TEST_DATA_DIR) / "raster-store-refactor/sf-flat";
    auto fixture = octree::open_folder_indexed(fixture_path);
    REQUIRE(fixture.has_value());
    auto loaded = fixture->load(octree::Id::root());
    REQUIRE(loaded.has_value());
    return std::move(loaded.value());
}

} // namespace

TEST_CASE("SF builder finalization writes and validates a valid index", "[sfbuilder][sf]") {
    TemporaryDirectory directory;
    auto storage_result = octree::open_folder(directory.path());
    REQUIRE(storage_result.has_value());
    auto storage = std::move(storage_result.value());
    REQUIRE(storage.save(octree::Id::root(), fixture_mesh()).has_value());

    CHECK(terrainbuilder::finalize_storage(storage).has_value());
    CHECK(std::filesystem::is_regular_file(directory.path() / "terrain.index"));
}

TEST_CASE(
    "SF builder finalization retains an invalid written index for diagnosis",
    "[sfbuilder][sf]") {
    TemporaryDirectory directory;
    auto storage_result = octree::open_folder(directory.path());
    REQUIRE(storage_result.has_value());
    auto storage = std::move(storage_result.value());
    const octree::Id root = octree::Id::root();
    const mesh::Simple mesh = fixture_mesh();
    REQUIRE(storage.save(root, mesh).has_value());
    REQUIRE(storage.save(root.child(0).value(), mesh).has_value());

    const auto finalized = terrainbuilder::finalize_storage(storage);
    REQUIRE_FALSE(finalized.has_value());
    REQUIRE(std::holds_alternative<sf::InvalidTopology>(finalized.error()));
    CHECK(std::get<sf::InvalidTopology>(finalized.error()).key == root);
    CHECK(std::filesystem::is_regular_file(directory.path() / "terrain.index"));

    auto reopened = octree::open_index(directory.path() / "terrain.index");
    REQUIRE(reopened.has_value());
    CHECK(reopened->index().is(store::NodeStatus::Inner, root).value());
}
