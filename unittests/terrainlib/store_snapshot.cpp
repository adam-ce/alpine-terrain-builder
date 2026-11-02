/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../attempts.h"
#include "../temporary_directory.h"
#include "io/byte.h"
#include "io/envelope.h"
#include "mesh/storage.h"
#include "octree/storage/snapshot.h"
#include "octree/store_layout/Mappings.h"
#include "store/inputs.h"
#include "store/snapshot.h"

namespace {

using test::TemporaryDirectory;

struct Record {
    std::string input;
    int setting = 0;

    bool operator==(const Record&) const = default;
};

using Schema = io::envelope::PayloadSchema<"test.Inputs", io::envelope::Version<1, Record>>;

mesh::Simple triangle() { return mesh::Simple({ { 0, 1, 2 } }, { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } }); }

Expected<mesh::storage::IndexedStorage> create_mesh_storage(const std::filesystem::path& final_path)
{
    return octree::storage::create<mesh::Simple>(
        final_path, std::string(mesh::storage::payload_class), ".sfmesh", mesh::codec::from_extension, octree::store_layout::flat());
}

} // namespace

TEST_CASE("snapshots are created as time-stamped attempts and published by renaming", "[store][snapshot]")
{
    TemporaryDirectory directory("snapshot");
    const auto final_path = directory.path() / "nested" / "output";
    CHECK(store::snapshot::attempts(final_path).value().empty());

    const auto lock_path = directory.path() / "nested" / "output.lock";
    auto first = store::snapshot::create_partial(final_path);
    REQUIRE(first.has_value());
    CHECK(std::filesystem::is_directory(first->path));
    CHECK(store::snapshot::is_partial(first->path));
    CHECK_FALSE(store::snapshot::is_partial(final_path));
    CHECK(first->path.filename().string().starts_with("output."));
    CHECK(first->path.filename().string().size() == std::string("output.yyyymmddThhmmss.mmmZ.part").size());
    CHECK(std::filesystem::exists(lock_path));
    // The lock excludes a concurrent run on the same output until it is released.
    const auto concurrent = store::snapshot::create_partial(final_path);
    REQUIRE_FALSE(concurrent.has_value());
    CHECK(concurrent.error().code() == Error::Code::AlreadyExists);
    first->lock = {};
    // Consecutive attempts get distinct times that sort chronologically.
    test::wait_for_next_attempt_time();
    auto second = store::snapshot::create_partial(final_path.string() + "/");
    REQUIRE(second.has_value());
    CHECK(first->path < second->path);

    const auto older = final_path.parent_path() / "output.20200101T000000.000Z.part";
    REQUIRE(std::filesystem::create_directory(older));
    const auto other = final_path.parent_path() / "output.v2.20200101T000000.000Z.part";
    for (const auto& ignored : { final_path.parent_path() / "output.part", other, final_path.parent_path() / "other.20200101T000000.000Z.part" }) {
        REQUIRE(std::filesystem::create_directory(ignored));
    }
    CHECK(store::snapshot::attempts(final_path).value() == std::vector { older, first->path, second->path });

    SECTION("a .part final path is rejected")
    {
        const auto rejected = store::snapshot::create_partial(first->path);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == Error::Code::InvalidInput);
    }

    SECTION("publication removes the input record and the lock, renames and deletes the other attempts")
    {
        REQUIRE(store::inputs::write<Schema>(second->path, Record { "input", 1 }).has_value());
        REQUIRE(store::snapshot::publish(second->path, std::move(second->lock)).has_value());
        CHECK(std::filesystem::is_directory(final_path));
        CHECK_FALSE(std::filesystem::exists(final_path / store::inputs::file_name));
        CHECK_FALSE(std::filesystem::exists(lock_path));
        CHECK(store::snapshot::attempts(final_path).value().empty());
        CHECK_FALSE(std::filesystem::exists(older));
        CHECK(std::filesystem::exists(other));

        const auto again = store::snapshot::create_partial(final_path);
        REQUIRE_FALSE(again.has_value());
        CHECK(again.error().code() == Error::Code::AlreadyExists);
        CHECK_FALSE(std::filesystem::exists(lock_path));
    }

    SECTION("publication does not replace an existing destination")
    {
        SECTION("empty directory") { REQUIRE(std::filesystem::create_directory(final_path)); }
        SECTION("nonempty directory") { REQUIRE(io::byte::write(io::byte::span {}, final_path / "keep", io::byte::WriteMode::Overwrite)); }
        SECTION("dangling symlink") { std::filesystem::create_symlink(directory.path() / "missing", final_path); }
        REQUIRE(store::inputs::write<Schema>(second->path, Record { "input", 1 }).has_value());
        const auto published = store::snapshot::publish(second->path, std::move(second->lock));
        REQUIRE_FALSE(published.has_value());
        CHECK(published.error().code() == Error::Code::AlreadyExists);
        // The retained attempts stay usable as caches.
        CHECK(store::inputs::validate_cache<Schema>(second->path, Record { "input", 1 }).has_value());
        CHECK(store::snapshot::attempts(final_path).value() == std::vector { older, first->path, second->path });
    }
}

TEST_CASE("caches must be .part snapshots with a matching input record", "[store][snapshot]")
{
    TemporaryDirectory directory("snapshot-cache");
    const Record record { "input", 1 };
    const auto cache = directory.path() / "cache.part";
    REQUIRE(std::filesystem::create_directory(cache));

    SECTION("missing record")
    {
        const auto validated = store::inputs::validate_cache<Schema>(cache, record);
        REQUIRE_FALSE(validated.has_value());
    }

    SECTION("matching record")
    {
        REQUIRE(store::inputs::write<Schema>(cache, record).has_value());
        CHECK(store::inputs::validate_cache<Schema>(cache, record).has_value());
        CHECK(store::inputs::validate_cache<Schema>(cache.string() + "/", record).has_value());
    }

    SECTION("different record")
    {
        REQUIRE(store::inputs::write<Schema>(cache, record).has_value());
        const auto validated = store::inputs::validate_cache<Schema>(cache, Record { "input", 2 });
        REQUIRE_FALSE(validated.has_value());
        CHECK(validated.error().code() == Error::Code::InvalidInput);
    }

    SECTION("not a .part snapshot")
    {
        const auto published = directory.path() / "published";
        REQUIRE(std::filesystem::create_directory(published));
        REQUIRE(store::inputs::write<Schema>(published, record).has_value());
        const auto validated = store::inputs::validate_cache<Schema>(published, record);
        REQUIRE_FALSE(validated.has_value());
        CHECK(validated.error().code() == Error::Code::InvalidInput);
    }
}

TEST_CASE("octree snapshots are created empty, published, and fingerprinted", "[store][snapshot][octree]")
{
    TemporaryDirectory directory("octree-snapshot");
    const auto final_path = directory.path() / "output";
    const octree::Id root = octree::Id::root();
    {
        auto created = create_mesh_storage(final_path);
        REQUIRE(created.has_value());
        auto storage = std::move(*created);
        CHECK(store::snapshot::attempts(final_path).value() == std::vector { storage.base_path() });
        CHECK(std::filesystem::is_regular_file(storage.base_path() / octree::storage::index_file_name));
        // The storage holds the lock of its output.
        CHECK_FALSE(create_mesh_storage(final_path).has_value());
        REQUIRE(storage.save(root, triangle()).has_value());
        REQUIRE(octree::storage::publish(std::move(storage)).has_value());
    }
    CHECK(store::snapshot::attempts(final_path).value().empty());
    CHECK_FALSE(std::filesystem::exists(directory.path() / "output.lock"));

    const auto reopened = mesh::storage::open_index(final_path / octree::storage::index_file_name);
    REQUIRE(reopened.has_value());
    CHECK(reopened->has(root).value());
    CHECK(reopened->codec_selector() == ".sfmesh");

    const auto existing = create_mesh_storage(final_path);
    REQUIRE_FALSE(existing.has_value());
    CHECK(existing.error().code() == Error::Code::AlreadyExists);

    const auto fingerprint = store::inputs::fingerprint(final_path, octree::storage::metadata_file_name, octree::storage::index_file_name);
    REQUIRE(fingerprint.has_value());
    CHECK(fingerprint->path == std::filesystem::canonical(final_path).string());
    CHECK(fingerprint->index_hash == io::envelope::read_header(final_path / octree::storage::index_file_name)->hash);
    CHECK(fingerprint->metadata_hash == io::envelope::read_header(final_path / octree::storage::metadata_file_name)->hash);

    const auto metadata_path = final_path / octree::storage::metadata_file_name;
    const auto metadata = io::envelope::read<octree::storage::StoreMetadataSchema>(metadata_path);
    REQUIRE(metadata.has_value());
    SECTION("recompression keeps the fingerprint")
    {
        REQUIRE(io::envelope::write<octree::storage::StoreMetadataSchema>(
            *metadata, metadata_path, io::byte::WriteMode::Overwrite, true, io::envelope::CompressionAlgorithm::None));
        const auto recompressed = store::inputs::fingerprint(final_path, octree::storage::metadata_file_name, octree::storage::index_file_name);
        REQUIRE(recompressed.has_value());
        CHECK(*recompressed == *fingerprint);
    }
    SECTION("files without a hash are rejected")
    {
        REQUIRE(io::envelope::write<octree::storage::StoreMetadataSchema>(*metadata,
            metadata_path,
            io::byte::WriteMode::Overwrite,
            true,
            io::envelope::CompressionAlgorithm::ZstdDefaultCompression,
            io::hash::Algorithm::None));
        const auto unhashed = store::inputs::fingerprint(final_path, octree::storage::metadata_file_name, octree::storage::index_file_name);
        REQUIRE_FALSE(unhashed.has_value());
        CHECK(unhashed.error().code() == Error::Code::Unsupported);
    }
}
