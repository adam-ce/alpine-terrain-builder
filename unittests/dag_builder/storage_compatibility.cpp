#include <filesystem>
#include <future>

#include <catch2/catch_test_macros.hpp>

#include "../temporary_directory.h"
#include "build.h"
#include "codec.h"
#include "dag_node.h"
#include "mesh/SimpleMesh.h"
#include "mesh/storage.h"
#include "storage.h"
#include "store/ThreadSafeStorage.h"

namespace {

using test::TemporaryDirectory;

dag::ClusterBatch sample_batch()
{
    dag::ClusterBatch batch;
    batch.metadata.group_assignment = { 0 };
    batch.metadata.groups = { {
        .children = { { octree::Id::root().child(4).value(), 3 } },
        .error = 12.5,
        .bounds = {},
    } };
    batch.clustering.positions = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
    };
    batch.clustering.clusters = { {
        .id = 17,
        .vertex_indices = { 0, 1, 2 },
        .local_triangles = { { 0, 1, 2 } },
        .uvs = {},
        .texture_id = std::nullopt,
        .absolute_error = 0.0,
    } };
    return batch;
}

mesh::Simple sample_mesh() { return mesh::Simple({ { 0, 1, 2 } }, { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } }); }

} // namespace

TEST_CASE("runtime DAG envelope codec is reentrant")
{
    const dag::ClusterBatch batch = sample_batch();

    TemporaryDirectory output;
    dag::codec::ClusterBatch codec;
    std::vector<std::future<bool>> operations;
    for (int index = 0; index < 8; ++index) {
        operations.push_back(std::async(std::launch::async, [&codec, &batch, &output, index] {
            const std::filesystem::path path(output.path() / std::to_string(index) / "node");
            if (!codec.write(path, batch).has_value()) {
                return false;
            }
            const auto loaded = codec.read(path);
            return loaded.has_value() && loaded->metadata.group_assignment == batch.metadata.group_assignment;
        }));
    }
    for (auto& operation : operations) {
        REQUIRE(operation.get());
    }
}

TEST_CASE("DAG codec rejects invalid batches and inconsistent files")
{
    TemporaryDirectory output;
    dag::codec::ClusterBatch codec;
    const std::filesystem::path node_path(output.path() / "node");
    const auto node_paths = codec.paths(node_path);

    SECTION("write")
    {
        auto batch = sample_batch();
        batch.metadata.group_assignment.clear();

        const auto result = codec.write(node_path, batch);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == Error::Code::InvalidInput);
        CHECK_FALSE(std::filesystem::exists(node_paths[0]));
        CHECK_FALSE(std::filesystem::exists(node_paths[1]));
    }

    SECTION("write invalid metadata")
    {
        auto batch = sample_batch();
        batch.metadata.group_assignment = { 1 };

        const auto result = codec.write(node_path, batch);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == Error::Code::InvalidInput);
        CHECK_FALSE(std::filesystem::exists(node_paths[0]));
        CHECK_FALSE(std::filesystem::exists(node_paths[1]));
    }

    SECTION("read")
    {
        REQUIRE(codec.write(node_path, sample_batch()).has_value());
        const std::filesystem::path empty_node_path(output.path() / "empty-node");
        const auto empty_node_paths = codec.paths(empty_node_path);
        REQUIRE(codec.write(empty_node_path, dag::ClusterBatch {}).has_value());
        REQUIRE(std::filesystem::copy_file(empty_node_paths[1], node_paths[1], std::filesystem::copy_options::overwrite_existing));

        const auto result = codec.read(node_path);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == Error::Code::CorruptData);
    }
}

TEST_CASE("DAG resolvers expose writable batches and read-only metadata", "[store][open]")
{
    const auto batch_codec = dag::codec::cluster_batch_from_extension(".dag");
    REQUIRE(batch_codec.has_value());
    CHECK(batch_codec.value()->paths("node") == std::vector<std::filesystem::path> { "node.dag", "node.dagmeta" });

    const auto metadata_codec = dag::codec::metadata_from_extension(".dag");
    REQUIRE(metadata_codec.has_value());
    CHECK(metadata_codec.value()->paths("node") == std::vector<std::filesystem::path> { "node.dagmeta" });
    const auto write_result = metadata_codec.value()->write("node", dag::NodeMetadata {});
    REQUIRE_FALSE(write_result.has_value());
    CHECK(write_result.error().code() == Error::Code::Unsupported);

    const auto unknown = dag::codec::cluster_batch_from_extension(".unknown");
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error().code() == Error::Code::Unsupported);
}

TEST_CASE("DAG indexed storage survives ThreadSafeStorage move and release", "[store][storage]")
{
    const dag::ClusterBatch batch = sample_batch();

    TemporaryDirectory output;
    auto storage_result = dag::storage::open_folder_indexed(output.path());
    REQUIRE(storage_result.has_value());
    store::ThreadSafeStorage synchronized(std::move(storage_result.value()));
    REQUIRE(synchronized.save(octree::Id::root(), batch).has_value());

    auto released = std::move(synchronized).release();
    REQUIRE(released.save_index().has_value());
    CHECK(std::filesystem::is_regular_file(output.path() / "octree.storeindex"));
    CHECK(std::filesystem::is_regular_file(output.path() / "octree.storemeta"));

    auto reopened_result = dag::storage::open_folder_indexed(output.path());
    REQUIRE(reopened_result.has_value());
    CHECK(reopened_result->has(octree::Id::root()).value());
    CHECK(reopened_result->load(octree::Id::root()).has_value());
    const auto full_paths = reopened_result->paths(octree::Id::root());
    REQUIRE(full_paths.has_value());
    REQUIRE(full_paths->size() == 2);

    auto metadata_result = dag::storage::open_metadata_indexed(output.path());
    REQUIRE(metadata_result.has_value());
    REQUIRE(std::filesystem::remove(full_paths->front()));
    const auto metadata = metadata_result->load(octree::Id::root());
    REQUIRE(metadata.has_value());
    CHECK(metadata->group_assignment == batch.metadata.group_assignment);
    CHECK(metadata->groups.front().error == batch.metadata.groups.front().error);
}

TEST_CASE("DAG storage hard-links both files for the same format", "[store][copy]")
{
    TemporaryDirectory source_directory;
    TemporaryDirectory target_directory;
    auto source_result = dag::storage::open_folder_indexed(source_directory.path());
    auto target_result = dag::storage::open_folder_indexed(target_directory.path());
    REQUIRE(source_result.has_value());
    REQUIRE(target_result.has_value());
    auto source = std::move(*source_result);
    auto target = std::move(*target_result);
    const octree::Id root = octree::Id::root();
    REQUIRE(source.save(root, sample_batch()).has_value());
    REQUIRE(target.copy_from(root, source).has_value());

    const auto source_paths = source.paths(root);
    const auto target_paths = target.paths(root);
    REQUIRE(source_paths.has_value());
    REQUIRE(target_paths.has_value());
    REQUIRE(source_paths->size() == 2);
    REQUIRE(target_paths->size() == 2);
    CHECK(std::filesystem::equivalent((*source_paths)[0], (*target_paths)[0]));
    CHECK(std::filesystem::equivalent((*source_paths)[1], (*target_paths)[1]));
}

TEST_CASE("DAG builder rejects invalid SF topology before processing", "[dag][sf][validation]")
{
    TemporaryDirectory input_directory;
    TemporaryDirectory output_directory;
    auto input_result = mesh::storage::open_folder_indexed(input_directory.path());
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
    const dag::BuildOptions options {
        .clusters_per_partition = 1,
        .target_ratio = 1.0f,
        .relative_target_error = std::nullopt,
    };

    const auto built = dag::build_full(input, output, options);
    REQUIRE_FALSE(built.has_value());
    CHECK(built.error().code() == Error::Code::CorruptData);
    CHECK(built.error().to_string().contains(root.to_string()));
    CHECK(output.index().empty());
}
