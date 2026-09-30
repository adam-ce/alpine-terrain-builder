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

#include "mesh/codec/Gltf.h"
#include "mesh/codec/SfMesh.h"
#include "octree/store_layout/Mappings.h"
#include "store/path_layout.h"

TEST_CASE("extensionless octree mappings preserve physical paths", "[store][layout]")
{
    const std::filesystem::path dataset = "dataset";
    mesh::codec::SfMesh sfmesh;

    SECTION("flat")
    {
        const store::path_layout::Resolver layout(dataset / "sf-flat", octree::store_layout::flat());
        const std::filesystem::path node_path = layout.node_path(octree::Id::root());
        CHECK(node_path == dataset / "sf-flat/0-0");
        CHECK(sfmesh.paths(node_path) == std::vector { dataset / "sf-flat/0-0.sfmesh" });
        CHECK(layout.key_from_node_path(node_path) == octree::Id::root());
    }

    SECTION("level and coordinate directories")
    {
        const store::path_layout::Resolver layout(dataset / "sf-coordinates", octree::store_layout::level_and_coordinate_directories());
        const octree::Id child = octree::Id::root().child(2).value();
        const octree::Id deep = octree::Id::root().child(5).value().child(7).value();
        CHECK(sfmesh.paths(layout.node_path(child)) == std::vector { dataset / "sf-coordinates/1/0/1/0.sfmesh" });
        CHECK(sfmesh.paths(layout.node_path(deep)) == std::vector { dataset / "sf-coordinates/2/3/1/3.sfmesh" });
        CHECK(layout.key_from_node_path(layout.node_path(child)) == child);
        CHECK(layout.key_from_node_path(layout.node_path(deep)) == deep);
    }
}

TEST_CASE("octree mapping lookup is explicit", "[store][layout]")
{
    REQUIRE(octree::store_layout::from_id("flat").has_value());
    CHECK(octree::store_layout::from_id("flat")->id == "flat");
    REQUIRE(octree::store_layout::from_id("level_and_coordinate_directories").has_value());
    CHECK(octree::store_layout::from_id("level_and_coordinate_directories")->id == "level_and_coordinate_directories");
    CHECK_FALSE(octree::store_layout::from_id("unknown").has_value());
    CHECK(octree::store_layout::all().size() == 2);
}

TEST_CASE("extensionless octree mappings validate complete paths", "[store][layout]")
{
    const auto flat = octree::store_layout::flat();
    CHECK_FALSE(flat.node_path_to_key("1-2.sfmesh").has_value());
    CHECK_FALSE(flat.node_path_to_key("nested/1-2").has_value());
    CHECK_FALSE(flat.node_path_to_key("1-2-extra").has_value());

    const auto coordinates = octree::store_layout::level_and_coordinate_directories();
    CHECK_FALSE(coordinates.node_path_to_key("1/0/0/0.sfmesh").has_value());
    CHECK_FALSE(coordinates.node_path_to_key("1/0/0").has_value());
    CHECK_FALSE(coordinates.node_path_to_key("1/0/0/0/extra").has_value());
    CHECK_FALSE(coordinates.node_path_to_key("22/0/0/0").has_value());
}

TEST_CASE("runtime mesh codecs own their filename endings", "[store][layout][codec]")
{
    const std::filesystem::path path("12/34/56/78");
    mesh::codec::SfMesh sfmesh;
    mesh::codec::Gltf binary(mesh::codec::GltfContainer::Binary);
    mesh::codec::Gltf json(mesh::codec::GltfContainer::Json);

    CHECK(sfmesh.paths(path) == std::vector<std::filesystem::path> { "12/34/56/78.sfmesh" });
    CHECK(binary.paths(path) == std::vector<std::filesystem::path> { "12/34/56/78.glb" });
    CHECK(json.paths(path) == std::vector<std::filesystem::path> { "12/34/56/78.gltf" });
}
