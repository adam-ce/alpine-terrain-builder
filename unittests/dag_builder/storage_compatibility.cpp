#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>

#include <catch2/catch_test_macros.hpp>

#include "encoded.h"
#include "build.h"
#include "io/bytes.h"
#include "io/serialize.h"
#include "octree/disk/IndexFile.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"
#include "storage.h"
#include "store/codec/ZppBits.h"
#include "thread_safe_storage.h"

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path()
            / ("atb-dag-codec-" + std::to_string(timestamp) + "-"
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

} // namespace

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

    auto batch_storage_result = dag::storage::open_folder_indexed(path);
    REQUIRE(batch_storage_result.has_value());
    const auto batch_storage = std::move(batch_storage_result.value());
    const auto batch = batch_storage.load(root);
    REQUIRE(batch.has_value());
    CHECK(batch->metadata.group_assignment == std::vector<uint32_t>{0});
    REQUIRE(batch->metadata.groups.size() == 1);
    CHECK(batch->metadata.groups.front().error == 12.5);
    CHECK(batch->metadata.groups.front().children == std::vector<dag::Id>{{root.child(4).value(), 3}});
    REQUIRE(batch->clustering.clusters.size() == 1);
    CHECK(batch->clustering.clusters.front().id == 17);

    auto metadata_storage_result = dag::storage::open_metadata_indexed(path);
    REQUIRE(metadata_storage_result.has_value());
    const auto metadata_storage = std::move(metadata_storage_result.value());
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

    store::codec::ZppBits<dag::ClusterBatch> runtime_codec;
    const auto runtime_batch = runtime_codec.read(store::NodePath(path / "0/0/0/0"));
    REQUIRE(runtime_batch.has_value());
    CHECK(runtime_batch->metadata.group_assignment == batch->metadata.group_assignment);
    CHECK(runtime_codec.paths(store::NodePath(path / "0/0/0/0"))
          == std::vector{path / "0/0/0/0.bin"});

    TemporaryDirectory output;
    const store::NodePath output_path(output.path() / "nested/node");
    REQUIRE(runtime_codec.write(output_path, batch.value()).has_value());
    const auto runtime_bytes = io::read_bytes_from_path(output.path() / "nested/node.bin");
    REQUIRE(runtime_bytes.has_value());
    CHECK(runtime_bytes.value() == fixture_bytes.value());
}

TEST_CASE("runtime DAG ZPP Bits codec is reentrant") {
    const std::filesystem::path fixture =
        std::filesystem::path(ALP_TEST_DATA_DIR)
        / "raster-store-refactor/dag/0/0/0/0.bin";
    const auto batch = io::read_from_path<dag::ClusterBatch>(fixture);
    REQUIRE(batch.has_value());

    TemporaryDirectory output;
    store::codec::ZppBits<dag::ClusterBatch> codec;
    std::vector<std::future<bool>> operations;
    for (int index = 0; index < 8; ++index) {
        operations.push_back(std::async(std::launch::async, [&codec, &batch, &output, index] {
            const store::NodePath path(output.path() / std::to_string(index) / "node");
            if (!codec.write(path, batch.value()).has_value()) {
                return false;
            }
            const auto loaded = codec.read(path);
            return loaded.has_value()
                && loaded->metadata.group_assignment == batch->metadata.group_assignment;
        }));
    }
    for (auto &operation : operations) {
        REQUIRE(operation.get());
    }
}

TEST_CASE("DAG resolvers expose writable batches and read-only metadata", "[store][open]") {
    const auto batch_codec = dag::codec::from_extension(".bin");
    REQUIRE(batch_codec.has_value());
    CHECK(batch_codec.value()->paths(store::NodePath("node"))
          == std::vector<std::filesystem::path>{"node.bin"});

    const auto metadata_codec = dag::codec::metadata_from_extension(".bin");
    REQUIRE(metadata_codec.has_value());
    const auto write_result = metadata_codec.value()->write(
        store::NodePath("node"),
        dag::NodeMetadata{});
    REQUIRE_FALSE(write_result.has_value());
    CHECK(write_result.error().operation == store::CodecOperation::Write);
    CHECK(write_result.error().category == store::CodecErrorCategory::UnsupportedOperation);

    const auto unknown = dag::codec::from_extension(".unknown");
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error().category == store::CodecErrorCategory::UnsupportedCodec);
}

TEST_CASE("DAG indexed storage survives ThreadSafeStorage move and release", "[store][storage]") {
    const std::filesystem::path fixture =
        std::filesystem::path(ALP_TEST_DATA_DIR)
        / "raster-store-refactor/dag/0/0/0/0.bin";
    const auto batch = io::read_from_path<dag::ClusterBatch>(fixture);
    REQUIRE(batch.has_value());

    TemporaryDirectory output;
    auto storage_result = dag::storage::open_folder_indexed(output.path());
    REQUIRE(storage_result.has_value());
    dag::ThreadSafeStorage synchronized(std::move(storage_result.value()));
    REQUIRE(synchronized.save(octree::Id::root(), batch.value()).has_value());

    auto released = std::move(synchronized).release();
    REQUIRE(released.save_index().has_value());
    CHECK(std::filesystem::is_regular_file(output.path() / "terrain.index"));

    auto reopened_result = dag::storage::open_folder_indexed(output.path());
    REQUIRE(reopened_result.has_value());
    CHECK(reopened_result->has(octree::Id::root()).value());
    CHECK(reopened_result->load(octree::Id::root()).has_value());
}

TEST_CASE("DAG builder rejects invalid SF topology before processing", "[dag][sf][validation]") {
    const std::filesystem::path fixture_path =
        std::filesystem::path(ALP_TEST_DATA_DIR) / "raster-store-refactor/sf-flat";
    auto fixture_result = octree::open_folder_indexed(fixture_path);
    REQUIRE(fixture_result.has_value());
    const auto mesh_result = fixture_result->load(octree::Id::root());
    REQUIRE(mesh_result.has_value());

    TemporaryDirectory input_directory;
    TemporaryDirectory output_directory;
    auto input_result = octree::open_folder_indexed(input_directory.path());
    REQUIRE(input_result.has_value());
    auto input = std::move(input_result.value());
    const octree::Id root = octree::Id::root();
    REQUIRE(input.save(root, mesh_result.value()).has_value());
    REQUIRE(input.save(root.child(0).value(), mesh_result.value()).has_value());
    REQUIRE(input.index().is(store::NodeStatus::Inner, root).value());

    auto output_result = dag::storage::open_folder_indexed(output_directory.path());
    REQUIRE(output_result.has_value());
    auto output = std::move(output_result.value());
    const dag::BuildOptions options{
        .clusters_per_partition = 1,
        .target_ratio = 1.0f,
        .relative_target_error = std::nullopt,
    };

    const auto built = dag::build_full(input, output, options);
    REQUIRE_FALSE(built.has_value());
    CHECK(built.error().key == root);
    CHECK(output.index().empty());
}
