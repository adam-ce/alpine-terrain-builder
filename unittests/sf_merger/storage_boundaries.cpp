#include <atomic>
#include <chrono>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "cut.h"
#include "merge.h"
#include "octree/storage/open.h"

namespace {

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string_view label)
    {
        static std::atomic_uint64_t counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("atb-sfmerger-" + std::string(label) + "-" + std::to_string(timestamp) + "-" + std::to_string(counter++));
        REQUIRE(std::filesystem::create_directories(m_path));
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

mesh::Simple triangle(const double x_offset = 0.0)
{
    return mesh::Simple({ { 0, 1, 2 } },
        {
            { -1.0 + x_offset, 0.0, 0.0 },
            { 1.0 + x_offset, 0.0, 0.0 },
            { x_offset, 2.0, 0.0 },
        });
}

mesh::Simple clipping_box()
{
    return mesh::Simple(
        {
            { 0, 1, 2 },
            { 0, 2, 3 },
            { 1, 5, 6 },
            { 1, 6, 2 },
            { 5, 4, 7 },
            { 5, 7, 6 },
            { 4, 0, 3 },
            { 4, 3, 7 },
            { 3, 2, 6 },
            { 3, 6, 7 },
            { 4, 5, 1 },
            { 4, 1, 0 },
        },
        {
            { 0.0, -1.0, -1.0 },
            { 2.0, -1.0, -1.0 },
            { 2.0, 3.0, -1.0 },
            { 0.0, 3.0, -1.0 },
            { 0.0, -1.0, 1.0 },
            { 2.0, -1.0, 1.0 },
            { 2.0, 3.0, 1.0 },
            { 0.0, 3.0, 1.0 },
        });
}

mesh::storage::IndexedStorage indexed_storage(const std::filesystem::path& path)
{
    auto result = octree::open_folder_indexed(path);
    REQUIRE(result.has_value());
    return std::move(result.value());
}

mesh::storage::Storage unindexed_storage(const std::filesystem::path& path)
{
    auto result = octree::open_folder(path);
    REQUIRE(result.has_value());
    return std::move(result.value());
}

} // namespace

TEST_CASE("SF merge rejects Inner input before dispatch", "[sfmerger][sf][validation]")
{
    TemporaryDirectory left_directory("invalid-left");
    TemporaryDirectory right_directory("invalid-right");
    TemporaryDirectory output_directory("invalid-output");
    auto left = indexed_storage(left_directory.path());
    auto right = indexed_storage(right_directory.path());
    auto output = unindexed_storage(output_directory.path());
    const octree::Id root = octree::Id::root();
    REQUIRE(left.save(root, triangle()).has_value());
    REQUIRE(left.save(root.child(0).value(), triangle()).has_value());

    const auto merged = merge_datasets(left, right, output);
    REQUIRE_FALSE(merged.has_value());
    REQUIRE(std::holds_alternative<sf::InvalidTopology>(merged.error()));
    CHECK(std::get<sf::InvalidTopology>(merged.error()).key == root);
    CHECK_FALSE(std::filesystem::exists(output_directory.path() / "octree.storeindex"));
}

TEST_CASE("SF merge retains an invalid output index for diagnosis", "[sfmerger][sf][validation]")
{
    TemporaryDirectory left_directory("valid-left");
    TemporaryDirectory right_directory("valid-right");
    TemporaryDirectory output_directory("bad-final-output");
    auto left = indexed_storage(left_directory.path());
    auto right = indexed_storage(right_directory.path());
    auto output = unindexed_storage(output_directory.path());
    const octree::Id root = octree::Id::root();
    REQUIRE(output.save(root, triangle()).has_value());
    REQUIRE(output.save(root.child(0).value(), triangle()).has_value());

    const auto merged = merge_datasets(left, right, output);
    REQUIRE_FALSE(merged.has_value());
    REQUIRE(std::holds_alternative<sf::InvalidTopology>(merged.error()));
    CHECK(std::get<sf::InvalidTopology>(merged.error()).key == root);
    CHECK(std::filesystem::is_regular_file(output_directory.path() / "octree.storeindex"));
}

TEST_CASE("SF merge hard-links unchanged nodes and writes changed nodes", "[sfmerger][sf][copy]")
{
    const octree::Id root = octree::Id::root();

    SECTION("unchanged")
    {
        TemporaryDirectory left_directory("unchanged-left");
        TemporaryDirectory right_directory("unchanged-right");
        TemporaryDirectory output_directory("unchanged-output");
        auto left = indexed_storage(left_directory.path());
        auto right = indexed_storage(right_directory.path());
        auto output = unindexed_storage(output_directory.path());
        REQUIRE(left.save(root, triangle()).has_value());

        REQUIRE(merge_datasets(left, right, output).has_value());
        REQUIRE(output.has(root).value());
        CHECK(std::filesystem::equivalent(left.paths(root)->front(), output.paths(root)->front()));
    }

    SECTION("changed")
    {
        TemporaryDirectory left_directory("changed-left");
        TemporaryDirectory right_directory("changed-right");
        TemporaryDirectory output_directory("changed-output");
        auto left = indexed_storage(left_directory.path());
        auto right = indexed_storage(right_directory.path());
        auto output = unindexed_storage(output_directory.path());
        REQUIRE(left.save(root, triangle()).has_value());
        REQUIRE(right.save(root, triangle(10.0)).has_value());

        REQUIRE(merge_datasets(left, right, output).has_value());
        REQUIRE(output.has(root).value());
        CHECK_FALSE(std::filesystem::equivalent(left.paths(root)->front(), output.paths(root)->front()));
        CHECK_FALSE(std::filesystem::equivalent(right.paths(root)->front(), output.paths(root)->front()));
        REQUIRE(output.load(root).has_value());
        CHECK(output.load(root)->face_count() == 2);
    }
}

TEST_CASE("SF cut hard-links unchanged leaves and writes clipped leaves", "[sfmerger][sf][copy]")
{
    const octree::Id root = octree::Id::root();

    SECTION("unchanged")
    {
        TemporaryDirectory input_directory("cut-unchanged-input");
        TemporaryDirectory output_directory("cut-unchanged-output");
        auto input = indexed_storage(input_directory.path());
        auto output = unindexed_storage(output_directory.path());
        REQUIRE(input.save(root, triangle()).has_value());

        REQUIRE(cut_dataset(input, MeshMask {}, output, false).has_value());
        REQUIRE(output.has(root).value());
        CHECK(std::filesystem::equivalent(input.paths(root)->front(), output.paths(root)->front()));
    }

    SECTION("clipped")
    {
        TemporaryDirectory input_directory("cut-clipped-input");
        TemporaryDirectory output_directory("cut-clipped-output");
        auto input = indexed_storage(input_directory.path());
        auto output = unindexed_storage(output_directory.path());
        REQUIRE(input.save(root, triangle()).has_value());

        REQUIRE(cut_dataset(input, MeshMask { .components = { clipping_box() } }, output, true).has_value());
        REQUIRE(output.has(root).value());
        CHECK_FALSE(std::filesystem::equivalent(input.paths(root)->front(), output.paths(root)->front()));
        const auto clipped = output.load(root);
        REQUIRE(clipped.has_value());
        CHECK_FALSE(clipped->is_empty());
    }
}
