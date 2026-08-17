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

mesh::Simple sample_mesh() {
    return mesh::Simple(
        {{0, 1, 2}},
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}});
}

} // namespace

TEST_CASE("SF builder finalization writes and validates a valid index", "[sfbuilder][sf]") {
    TemporaryDirectory directory;
    auto storage_result = octree::open_folder(directory.path());
    REQUIRE(storage_result.has_value());
    auto storage = std::move(storage_result.value());
    REQUIRE(storage.save(octree::Id::root(), sample_mesh()).has_value());

    CHECK(terrainbuilder::finalize_storage(storage).has_value());
    CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storeindex"));
    CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storemeta"));
}

TEST_CASE(
    "SF builder finalization retains an invalid written index for diagnosis",
    "[sfbuilder][sf]") {
    TemporaryDirectory directory;
    auto storage_result = octree::open_folder(directory.path());
    REQUIRE(storage_result.has_value());
    auto storage = std::move(storage_result.value());
    const octree::Id root = octree::Id::root();
    const mesh::Simple mesh = sample_mesh();
    REQUIRE(storage.save(root, mesh).has_value());
    REQUIRE(storage.save(root.child(0).value(), mesh).has_value());

    const auto finalized = terrainbuilder::finalize_storage(storage);
    REQUIRE_FALSE(finalized.has_value());
    REQUIRE(std::holds_alternative<sf::InvalidTopology>(finalized.error()));
    CHECK(std::get<sf::InvalidTopology>(finalized.error()).key == root);
    CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storeindex"));

    auto reopened = octree::open_index(directory.path() / "octree.storeindex");
    REQUIRE(reopened.has_value());
    CHECK(reopened->index().is(store::NodeStatus::Inner, root).value());
}
