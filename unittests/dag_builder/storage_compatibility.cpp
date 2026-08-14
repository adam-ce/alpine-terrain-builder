#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>

#include <catch2/catch_test_macros.hpp>

#include "build.h"
#include "dag_node.h"
#include "mesh/SimpleMesh.h"
#include "octree/storage/open.h"
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

dag::ClusterBatch sample_batch() {
    dag::ClusterBatch batch;
    batch.metadata.group_assignment = {0};
    batch.metadata.groups = {{
        .children = {{octree::Id::root().child(4).value(), 3}},
        .error = 12.5,
        .bounds = {},
    }};
    batch.clustering.positions = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    batch.clustering.clusters = {{
        .id = 17,
        .vertex_indices = {0, 1, 2},
        .local_triangles = {{0, 1, 2}},
        .uvs = {},
        .texture_id = std::nullopt,
        .absolute_error = 0.0,
    }};
    return batch;
}

mesh::Simple sample_mesh() {
    return mesh::Simple(
        {{0, 1, 2}},
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}});
}

} // namespace

TEST_CASE("runtime DAG ZPP Bits codec is reentrant") {
    const dag::ClusterBatch batch = sample_batch();

    TemporaryDirectory output;
    store::codec::ZppBits<dag::ClusterBatch> codec;
    std::vector<std::future<bool>> operations;
    for (int index = 0; index < 8; ++index) {
        operations.push_back(std::async(std::launch::async, [&codec, &batch, &output, index] {
            const store::NodePath path(output.path() / std::to_string(index) / "node");
            if (!codec.write(path, batch).has_value()) {
                return false;
            }
            const auto loaded = codec.read(path);
            return loaded.has_value()
                && loaded->metadata.group_assignment == batch.metadata.group_assignment;
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
    const dag::ClusterBatch batch = sample_batch();

    TemporaryDirectory output;
    auto storage_result = dag::storage::open_folder_indexed(output.path());
    REQUIRE(storage_result.has_value());
    dag::ThreadSafeStorage synchronized(std::move(storage_result.value()));
    REQUIRE(synchronized.save(octree::Id::root(), batch).has_value());

    auto released = std::move(synchronized).release();
    REQUIRE(released.save_index().has_value());
    CHECK(std::filesystem::is_regular_file(output.path() / "terrain.index"));

    auto reopened_result = dag::storage::open_folder_indexed(output.path());
    REQUIRE(reopened_result.has_value());
    CHECK(reopened_result->has(octree::Id::root()).value());
    CHECK(reopened_result->load(octree::Id::root()).has_value());

    auto metadata_result = dag::storage::open_metadata_indexed(output.path());
    REQUIRE(metadata_result.has_value());
    const auto metadata = metadata_result->load(octree::Id::root());
    REQUIRE(metadata.has_value());
    CHECK(metadata->group_assignment == batch.metadata.group_assignment);
    CHECK(metadata->groups.front().error == batch.metadata.groups.front().error);
}

TEST_CASE("DAG builder rejects invalid SF topology before processing", "[dag][sf][validation]") {
    TemporaryDirectory input_directory;
    TemporaryDirectory output_directory;
    auto input_result = octree::open_folder_indexed(input_directory.path());
    REQUIRE(input_result.has_value());
    auto input = std::move(input_result.value());
    const octree::Id root = octree::Id::root();
    const mesh::Simple mesh = sample_mesh();
    REQUIRE(input.save(root, mesh).has_value());
    REQUIRE(input.save(root.child(0).value(), mesh).has_value());
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
