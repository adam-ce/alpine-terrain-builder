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

#include <catch2/catch_test_macros.hpp>

#include "../temporary_directory.h"
#include "mesh/storage.h"
#include "sf/finalize_storage.h"

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
