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

#include <fmt/core.h>

#include "../catch2_helpers.h"
#include "mesh/SimpleMesh.h"
#include "mesh/merge.h"
#include "mesh/utils.h"

TEST_CASE("mesh::merge") {
    SECTION("two tris with shared edge") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 1, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));

        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(1, 0, 0));
        mesh2.positions.push_back(glm::dvec3(1, 1, 0));
        mesh2.positions.push_back(glm::dvec3(0, 1, 0));

        mesh2.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh actual = mesh::merge(mesh1, mesh2, 0.1);

        SimpleMesh expected;
        expected.positions.push_back(glm::dvec3(0, 0, 0));
        expected.positions.push_back(glm::dvec3(1, 1, 0));
        expected.positions.push_back(glm::dvec3(1, 0, 0));
        expected.positions.push_back(glm::dvec3(0, 1, 0));

        expected.triangles.push_back(glm::uvec3(0, 1, 2));
        expected.triangles.push_back(glm::uvec3(2, 1, 3));

        sort_and_normalize_triangles(actual.triangles);
        sort_and_normalize_triangles(expected.triangles);

        CHECK(expected.positions == actual.positions);
        CHECK(expected.uvs == actual.uvs);
        CHECK(expected.triangles == actual.triangles);
    }

    SECTION("two tris with uvs") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 1, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));
        mesh1.uvs.push_back(glm::dvec2(0, 0));
        mesh1.uvs.push_back(glm::dvec2(1, 1));
        mesh1.uvs.push_back(glm::dvec2(1, 0));
        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(1, 0, 0));
        mesh2.positions.push_back(glm::dvec3(1, 1, 0));
        mesh2.positions.push_back(glm::dvec3(0, 1, 0));
        mesh2.uvs.push_back(glm::dvec2(1, 0));
        mesh2.uvs.push_back(glm::dvec2(1, 1));
        mesh2.uvs.push_back(glm::dvec2(0, 1));
        mesh2.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh actual = mesh::merge(mesh1, mesh2, 0.1);

        SimpleMesh expected;
        expected.positions.push_back(glm::dvec3(0, 0, 0));
        expected.positions.push_back(glm::dvec3(1, 1, 0));
        expected.positions.push_back(glm::dvec3(1, 0, 0));
        expected.positions.push_back(glm::dvec3(0, 1, 0));
        expected.uvs.push_back(glm::dvec2(0, 0));
        expected.uvs.push_back(glm::dvec2(1, 1));
        expected.uvs.push_back(glm::dvec2(1, 0));
        expected.uvs.push_back(glm::dvec2(0, 1));
        expected.triangles.push_back(glm::uvec3(0, 1, 2));
        expected.triangles.push_back(glm::uvec3(2, 1, 3));

        sort_and_normalize_triangles(actual.triangles);
        sort_and_normalize_triangles(expected.triangles);

        CHECK(expected.positions == actual.positions);
        CHECK(expected.uvs == actual.uvs);
        CHECK(expected.triangles == actual.triangles);
    }

    SECTION("distance epsilon") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));
        mesh1.positions.push_back(glm::dvec3(0, 1, 0));
        mesh1.positions.push_back(glm::dvec3(1, 1, 0));
        mesh1.triangles.push_back(glm::uvec3(0, 2, 1));
        mesh1.triangles.push_back(glm::uvec3(1, 2, 3));

        SimpleMesh mesh2;
        mesh2.positions.push_back(glm::dvec3(0.91, 0, 0));
        mesh2.positions.push_back(glm::dvec3(2, 0, 0));
        mesh2.positions.push_back(glm::dvec3(1.11, 1, 0));
        mesh2.positions.push_back(glm::dvec3(2, 1, 0));
        mesh2.triangles.push_back(glm::uvec3(0, 2, 1));
        mesh2.triangles.push_back(glm::uvec3(1, 2, 3));

        SimpleMesh actual = mesh::merge(mesh1, mesh2, 0.1);

        SimpleMesh expected;
        expected.positions.push_back(glm::dvec3(0, 0, 0));
        expected.positions.push_back(glm::dvec3(0.91, 0, 0));
        expected.positions.push_back(glm::dvec3(0, 1, 0));
        expected.positions.push_back(glm::dvec3(1, 1, 0));

        expected.positions.push_back(glm::dvec3(2, 0, 0));
        expected.positions.push_back(glm::dvec3(1.11, 1, 0));
        expected.positions.push_back(glm::dvec3(2, 1, 0));

        expected.triangles.push_back(glm::uvec3(0, 2, 1));
        expected.triangles.push_back(glm::uvec3(1, 2, 3));

        expected.triangles.push_back(glm::uvec3(1, 5, 4));
        expected.triangles.push_back(glm::uvec3(4, 5, 6));

        sort_and_normalize_triangles(actual.triangles);
        sort_and_normalize_triangles(expected.triangles);

        CHECK(expected.positions == actual.positions);
        CHECK(expected.uvs == actual.uvs);
        CHECK(expected.triangles == actual.triangles);
    }

    SECTION("merge with empty mesh") {
        SimpleMesh mesh1;
        mesh1.positions.push_back(glm::dvec3(0, 0, 0));
        mesh1.positions.push_back(glm::dvec3(1, 0, 0));
        mesh1.positions.push_back(glm::dvec3(0, 1, 0));
        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2; // empty

        SimpleMesh actual = mesh::merge(mesh1, mesh2, 0.1);

        CHECK(mesh1.positions == actual.positions);
        CHECK(mesh1.uvs == actual.uvs);
        CHECK(mesh1.triangles == actual.triangles);
    }

    /*
    TODO: this currently fails since we dont deduplicate the triangles
    SECTION("identical meshes collapse to one") {
        SimpleMesh mesh1;
        mesh1.positions = {
            glm::dvec3(0, 0, 0),
            glm::dvec3(1, 0, 0),
            glm::dvec3(0, 1, 0)};
        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2 = mesh1;

        SimpleMesh actual = mesh::merge(mesh1, mesh2, 0.001);

        CHECK(actual.positions == mesh1.positions);
        CHECK(actual.triangles == mesh1.triangles);
    }*/

    SECTION("epsilon too small prevents merging") {
        SimpleMesh mesh1;
        mesh1.positions = {
            glm::dvec3(0, 0, 0),
            glm::dvec3(1, 0, 0),
            glm::dvec3(0, 1, 0)};
        mesh1.triangles.push_back(glm::uvec3(0, 1, 2));

        SimpleMesh mesh2;
        mesh2.positions = {
            glm::dvec3(1.05, 0, 0), // within large epsilon, not small
            glm::dvec3(2, 0, 0),
            glm::dvec3(1, 1, 0)};
        mesh2.triangles.push_back(glm::uvec3(0, 1, 2));

        auto merged_small = mesh::merge(mesh1, mesh2, 0.01);
        auto merged_large = mesh::merge(mesh1, mesh2, 0.1);

        CHECK(merged_small.positions.size() > merged_large.positions.size());
    }
}