#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "io/serialize.h"
#include "mesh/SimpleMesh.h"
#include "octree/Storage.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/layout/strategy/Flat.h"
#include "octree/disk/layout/strategy/LevelAndCoordinateDirectories.h"
#include "octree/storage/RawStorage.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"

namespace {

std::filesystem::path fixture_path(const std::string_view name) {
    return std::filesystem::path(ALP_TEST_DATA_DIR) / "raster-store-refactor" / name;
}

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string_view label) {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path()
            / ("atb-raster-store-" + std::string(label) + "-"
               + std::to_string(timestamp) + "-" + std::to_string(counter++));
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

mesh::Simple triangle_mesh(const double offset) {
    return mesh::Simple(
        {{0, 1, 2}},
        {{offset, 0.0, 0.0}, {offset + 1.0, 0.0, 0.0}, {offset, 1.0, 0.0}});
}

octree::IndexedStorage make_storage(
    const std::filesystem::path &path,
    std::unique_ptr<octree::disk::layout::Strategy> strategy,
    const std::string &extension) {
    octree::disk::Layout layout(path, std::move(strategy), extension);
    return octree::IndexedStorage(octree::RawStorage_(std::move(layout)), octree::IndexMap{});
}

} // namespace

TEST_CASE("pre-refactor SF fixtures preserve index and path contracts") {
    const octree::Id root = octree::Id::root();

    SECTION("flat") {
        const std::filesystem::path path = fixture_path("sf-flat");
        const auto index_file = io::read_from_path<octree::disk::v1::IndexFile>(path / "terrain.index");
        REQUIRE(index_file.has_value());
        CHECK(index_file->layout_strategy_id == "flat");
        CHECK(index_file->preferred_extension == ".terrain");
        CHECK(index_file->map.size() == 1);
        CHECK(index_file->map.is(octree::NodeStatus::Leaf, root));
        CHECK(std::filesystem::is_regular_file(path / "0-0.terrain"));

        const octree::IndexedStorage storage = octree::open_folder_indexed(path);
        const auto mesh = storage.load(root);
        REQUIRE(mesh.has_value());
        CHECK(mesh->face_count() == 1);
    }

    SECTION("level and coordinate directories") {
        const std::filesystem::path path = fixture_path("sf-coordinates");
        const octree::Id child = root.child(2).value();
        const octree::Id deep = root.child(5).value().child(7).value();
        const octree::Id deep_parent = deep.parent().value();

        const auto index_file = io::read_from_path<octree::disk::v1::IndexFile>(path / "terrain.index");
        REQUIRE(index_file.has_value());
        CHECK(index_file->layout_strategy_id == "level_and_coordinate_directories");
        CHECK(index_file->preferred_extension == ".terrain");
        CHECK(index_file->map.is(octree::NodeStatus::Virtual, root));
        CHECK(index_file->map.is(octree::NodeStatus::Leaf, child));
        CHECK(index_file->map.is(octree::NodeStatus::Virtual, deep_parent));
        CHECK(index_file->map.is(octree::NodeStatus::Leaf, deep));
        CHECK(std::filesystem::is_regular_file(path / "1/0/1/0.terrain"));
        CHECK(std::filesystem::is_regular_file(path / "2/3/1/3.terrain"));

        std::vector<octree::Id> visited;
        octree::traverse(index_file->map, [&](const octree::Id id, const octree::NodeStatus) {
            visited.push_back(id);
        });
        CHECK(visited == std::vector{octree::Id{root}, child, deep_parent, deep});
    }
}

TEST_CASE("pre-refactor layouts round-trip boundary IDs") {
    const std::vector ids{
        octree::Id::root(),
        octree::Id(octree::Id::max_level(), octree::Id::Index{0}),
        octree::Id(octree::Id::max_level(), octree::Id::max_index_on_level(octree::Id::max_level())),
    };

    octree::disk::layout::strategy::Flat flat;
    octree::disk::layout::strategy::LevelAndCoordinateDirectories coordinates;
    for (const octree::Id id : ids) {
        const auto flat_path = flat.get_relative_node_path(id, ".terrain");
        CHECK(flat.get_id_from_relative_node_path(flat_path) == id);

        const auto coordinate_path = coordinates.get_relative_node_path(id, ".terrain");
        CHECK(coordinates.get_id_from_relative_node_path(coordinate_path) == id);
    }
}

TEST_CASE("pre-refactor storage hard-links matching payload formats") {
    TemporaryDirectory source_directory("hard-link-source");
    TemporaryDirectory target_directory("hard-link-target");
    auto source = make_storage(
        source_directory.path(),
        std::make_unique<octree::disk::layout::strategy::Flat>(),
        ".terrain");
    auto target = make_storage(
        target_directory.path(),
        std::make_unique<octree::disk::layout::strategy::Flat>(),
        ".terrain");

    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, triangle_mesh(0.0)).has_value());
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK(std::filesystem::equivalent(source.path_for(root), target.path_for(root)));
    REQUIRE(source.save_index().has_value());
    REQUIRE(target.save_index().has_value());
}

TEST_CASE("pre-refactor storage re-encodes differing payload formats") {
    TemporaryDirectory source_directory("reencode-source");
    TemporaryDirectory target_directory("reencode-target");
    auto source = make_storage(
        source_directory.path(),
        std::make_unique<octree::disk::layout::strategy::Flat>(),
        ".terrain");
    auto target = make_storage(
        target_directory.path(),
        std::make_unique<octree::disk::layout::strategy::Flat>(),
        ".glb");

    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, triangle_mesh(4.0)).has_value());
    REQUIRE(target.copy_from(root, source).has_value());
    CHECK_FALSE(std::filesystem::equivalent(source.path_for(root), target.path_for(root)));
    const auto loaded = target.load(root);
    REQUIRE(loaded.has_value());
    CHECK(loaded->face_count() == 1);
    REQUIRE(source.save_index().has_value());
    REQUIRE(target.save_index().has_value());
}

TEST_CASE("pre-refactor storage supports enabled overwrites") {
    TemporaryDirectory directory("overwrite");
    auto storage = make_storage(
        directory.path(),
        std::make_unique<octree::disk::layout::strategy::Flat>(),
        ".terrain");
    storage.settings().allow_overwrite = true;

    const octree::Id root = octree::Id::root();
    REQUIRE(storage.save(root, triangle_mesh(1.0)).has_value());
    REQUIRE(storage.save(root, triangle_mesh(9.0)).has_value());
    const auto loaded = storage.load(root);
    REQUIRE(loaded.has_value());
    REQUIRE_FALSE(loaded->positions.empty());
    CHECK(loaded->positions.front().x == 9.0);
    REQUIRE(storage.save_index().has_value());
}

TEST_CASE("pre-refactor folder opening scans payloads and creates an index") {
    TemporaryDirectory directory("folder-open");
    octree::OpenOptions options;
    options.default_layout_strategy = std::make_unique<octree::disk::layout::strategy::Flat>();
    options.preferred_extension_with_dot = ".terrain";

    {
        auto storage = octree::open_folder(directory.path(), false, std::move(options));
        CHECK_FALSE(storage.is_indexed());
        REQUIRE(storage.save(octree::Id::root(), triangle_mesh(2.0)).has_value());
        CHECK_FALSE(std::filesystem::exists(directory.path() / "terrain.index"));
    }

    {
        const auto storage = octree::open_folder_indexed(directory.path());
        CHECK(storage.is_indexed());
        CHECK(storage.has(octree::Id::root()));
        CHECK(std::filesystem::is_regular_file(directory.path() / "terrain.index"));
    }

    const auto reopened = octree::open_index<>(directory.path() / "terrain.index");
    REQUIRE(reopened.has_value());
    CHECK(reopened->has(octree::Id::root()));
}
