#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "mesh/storage.h"
#include "sf/finalize_storage.h"
#include "../temporary_directory.h"

namespace {

using test::TemporaryDirectory;

mesh::Simple sample_mesh() { return mesh::Simple({ { 0, 1, 2 } }, { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } }); }

} // namespace

TEST_CASE("SF builder finalization writes and validates a valid index", "[sfbuilder][sf]")
{
    TemporaryDirectory directory;
    auto storage_result = mesh::storage::open_folder(directory.path());
    REQUIRE(storage_result.has_value());
    auto storage = std::move(storage_result.value());
    REQUIRE(storage.save(octree::Id::root(), sample_mesh()).has_value());

    CHECK(sf::finalize_storage(storage).has_value());
    CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storeindex"));
    CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storemeta"));
}

TEST_CASE("SF builder finalization retains an invalid written index for diagnosis", "[sfbuilder][sf]")
{
    TemporaryDirectory directory;
    auto storage_result = mesh::storage::open_folder(directory.path());
    REQUIRE(storage_result.has_value());
    auto storage = std::move(storage_result.value());
    const octree::Id root = octree::Id::root();
    const mesh::Simple mesh = sample_mesh();
    REQUIRE(storage.save(root, mesh).has_value());
    REQUIRE(storage.save(root.child(0).value(), mesh).has_value());

    const auto finalized = sf::finalize_storage(storage);
    REQUIRE_FALSE(finalized.has_value());
    CHECK(finalized.error().code() == Error::Code::CorruptData);
    CHECK(finalized.error().to_string().contains(root.to_string()));
    CHECK(std::filesystem::is_regular_file(directory.path() / "octree.storeindex"));

    auto reopened = mesh::storage::open_index(directory.path() / "octree.storeindex");
    REQUIRE(reopened.has_value());
    CHECK(reopened->index().is(store::NodeStatus::Inner, root).value());
}
