/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <last name at cg tuwien ac at>
 * Copyright (C) 2022 alpinemaps.org
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

#include "../catch2_helpers.h"
#include "mesh/SimpleMesh.h"
#include "mesh/merging/mapping.h"
#include <fmt/core.h>
#include "spatial_lookup/CellBasedStorage.h"
#include "spatial_lookup/GridStorage.h"
#include "spatial_lookup/HashmapStorage.h"

using mesh::merging::VertexId;
using mesh::merging::VertexMapping;

TEST_CASE("merging::create_mapping") {
    SECTION("minimum test") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(0, 0, 0));

        std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};

        VertexMapping mapping = mesh::merging::create_mapping(meshes, 0.1);

        size_t idx1 = mapping.map_forward(VertexId{0, 0});
        size_t idx2 = mapping.map_forward(VertexId{1, 0});
        CHECK(idx1 == idx2);
    }

    SECTION("two identical positions are merged") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 1, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(1, 0, 0)); // identical to mesh1[2]
        mesh2.positions.push_back(glm::dvec3(1, 1, 0)); // identical to mesh1[1]
        mesh2.positions.push_back(glm::dvec3(0, 1, 0));

        std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};

        VertexMapping mapping = mesh::merging::create_mapping(meshes, 0.1);

        // Merged index of (1, 0, 0) should be the same for both meshes
        size_t idx1 = mapping.map_forward(VertexId{0, 2});
        size_t idx2 = mapping.map_forward(VertexId{1, 0});
        CHECK(idx1 == idx2);

        // Merged index of (1,1,0) should be the same for both
        size_t idx3 = mapping.map_forward(VertexId{0, 1});
        size_t idx4 = mapping.map_forward(VertexId{1, 1});
        CHECK(idx3 == idx4);

        // (0, 1, 0) should be unique
        size_t idx5 = mapping.map_forward(VertexId{1, 2});
        CHECK(idx5 != idx1);
        CHECK(idx5 != idx3);

        // Total unique merged vertices should be 4
        CHECK(mapping.find_max_merged_index() + 1 == 4);
    }

    SECTION("similar vertices within epsilon are merged") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(1.05, 0, 0)); // should be merged if epsilon >= 0.1
        mesh2.positions.push_back(glm::dvec3(2, 0, 0));

        std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};

        VertexMapping mapping = mesh::merging::create_mapping(meshes, 0.1);

        size_t idx1 = mapping.map_forward(VertexId{0, 1}); // (1, 0, 0)
        size_t idx2 = mapping.map_forward(VertexId{1, 0}); // (1.05, 0, 0)

        // They are within epsilon => should be merged
        CHECK(idx1 == idx2);

        // The far point should be separate
        size_t idx3 = mapping.map_forward(VertexId{1, 1}); // (2, 0, 0)
        CHECK(idx3 != idx1);

        CHECK(mapping.find_max_merged_index() + 1 == 3);
    }

    SECTION("similar vertices are NOT merged if outside epsilon") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));
        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(1.10000001, 0, 0));

        std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};

        VertexMapping mapping = mesh::merging::create_mapping(meshes, 0.1); // epsilon too small

        size_t idx1 = mapping.map_forward(VertexId{0, 0});
        size_t idx2 = mapping.map_forward(VertexId{1, 0});

        CHECK(idx1 != idx2);
        CHECK(mapping.find_max_merged_index() + 1 == 2);
    }

    SECTION("identity mapping on disjoint meshes") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(2, 0, 0));
        mesh2.positions.push_back(glm::dvec3(3, 0, 0));

        std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};

        VertexMapping mapping = mesh::merging::create_mapping(meshes, 0.1); // all far apart

        CHECK(mapping.find_max_merged_index() + 1 == 4);
        CHECK(mapping.map_forward(VertexId{0, 0}) != mapping.map_forward(VertexId{0, 1}));
        CHECK(mapping.map_forward(VertexId{1, 0}) != mapping.map_forward(VertexId{1, 1}));
    }

    SECTION("identical meshes collapse to one") {
        SimpleMesh mesh1;
        mesh1.positions = {
            glm::dvec3(0, 0, 0),
            glm::dvec3(1, 0, 0),
            glm::dvec3(0, 1, 0)};
        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2 = mesh1;

        std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {mesh1, mesh2};

        VertexMapping mapping = mesh::merging::create_mapping(meshes, 0.1);

        CHECK(mapping.map_forward(VertexId{0, 0}) == mapping.map_forward(VertexId{1, 0}));
        CHECK(mapping.map_forward(VertexId{0, 1}) == mapping.map_forward(VertexId{1, 1}));
        CHECK(mapping.map_forward(VertexId{0, 2}) == mapping.map_forward(VertexId{1, 2}));
    }
}
