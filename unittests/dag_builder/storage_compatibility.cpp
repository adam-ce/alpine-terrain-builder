#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "encoded.h"
#include "io/bytes.h"
#include "io/serialize.h"
#include "octree/disk/IndexFile.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"
#include "storage.h"

TEST_CASE("pre-refactor DAG fixture preserves index and payload contracts") {
    const std::filesystem::path path =
        std::filesystem::path(ALP_TEST_DATA_DIR) / "raster-store-refactor" / "dag";
    const octree::Id root = octree::Id::root();
    const octree::Id parent = root.child(3).value();
    const octree::Id leaf = parent.child(6).value();

    const auto index_file = io::read_from_path<octree::disk::v1::IndexFile>(path / "terrain.index");
    REQUIRE(index_file.has_value());
    CHECK(index_file->layout_strategy_id == "level_and_coordinate_directories");
    CHECK(index_file->preferred_extension == ".bin");
    CHECK(index_file->map.is(octree::NodeStatus::Inner, root));
    CHECK(index_file->map.is(octree::NodeStatus::Virtual, parent));
    CHECK(index_file->map.is(octree::NodeStatus::Leaf, leaf));
    CHECK(std::filesystem::is_regular_file(path / "0/0/0/0.bin"));
    CHECK(std::filesystem::is_regular_file(path / "2/2/3/1.bin"));

    std::vector<octree::Id> visited;
    octree::traverse(index_file->map, [&](const octree::Id id, const octree::NodeStatus) {
        visited.push_back(id);
    });
    CHECK(visited == std::vector<octree::Id>{root, parent, leaf});

    const auto batch_storage = octree::open_folder_indexed<dag::ClusterBatch>(path);
    const auto batch = batch_storage.load(root);
    REQUIRE(batch.has_value());
    CHECK(batch->metadata.group_assignment == std::vector<uint32_t>{0});
    REQUIRE(batch->metadata.groups.size() == 1);
    CHECK(batch->metadata.groups.front().error == 12.5);
    CHECK(batch->metadata.groups.front().children == std::vector<dag::Id>{{root.child(4).value(), 3}});
    REQUIRE(batch->clustering.clusters.size() == 1);
    CHECK(batch->clustering.clusters.front().id == 17);

    const auto metadata_storage = octree::open_folder_indexed<dag::NodeMetadata>(path);
    const auto metadata = metadata_storage.load(root);
    REQUIRE(metadata.has_value());
    CHECK(metadata->group_assignment == batch->metadata.group_assignment);
    REQUIRE(metadata->groups.size() == batch->metadata.groups.size());
    CHECK(metadata->groups.front().error == batch->metadata.groups.front().error);

    const auto encoded = io::write_to_bytes(batch.value());
    const auto fixture_bytes = io::read_bytes_from_path(path / "0/0/0/0.bin");
    REQUIRE(encoded.has_value());
    REQUIRE(fixture_bytes.has_value());
    CHECK(encoded.value() == fixture_bytes.value());
}
