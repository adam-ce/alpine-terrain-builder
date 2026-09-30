/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "mesh/SimpleMesh.h"
#include "mesh/reindex.h"

TEST_CASE("mesh::reindex") {
    using Catch::Matchers::UnorderedEquals;

    SimpleMesh mesh;
    mesh.positions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 1.0, 1.0),
        glm::dvec3(2.0, 2.0, 2.0),
        glm::dvec3(3.0, 3.0, 3.0),
        glm::dvec3(4.0, 4.0, 4.0)};
    mesh.uvs = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(1.0, 1.0),
        glm::dvec2(2.0, 2.0),
        glm::dvec2(3.0, 3.0),
        glm::dvec2(4.0, 4.0)};
    mesh.triangles = {
        glm::uvec3(1, 4, 3),
        glm::uvec3(1, 0, 3),
    };
    mesh.texture = cv::Mat3b(100, 100);

    auto run_checks = [&](SimpleMesh &original, SimpleMesh &reindexed) {
        CHECK(original.positions != reindexed.positions);
        CHECK(original.triangles != reindexed.triangles);

        CHECK(reindexed.positions.size() == 4);
        CHECK(reindexed.uvs.size() == 4);
        CHECK(reindexed.triangles.size() == original.triangles.size());

        for (uint32_t i = 0; i < original.triangles.size(); i++) {
            const glm::uvec3 &triangle = original.triangles[i];
            const glm::uvec3 &reindexed_triangle = reindexed.triangles[i];
            for (uint32_t j = 0; j < 3; j++) {
                const uint32_t original_index = triangle[j];
                const uint32_t new_index = reindexed_triangle[j];
                CHECK(original.positions[original_index] == reindexed.positions[new_index]);
                CHECK(original.uvs[original_index] == reindexed.uvs[new_index]);
            }
        }
        CHECK_THAT(reindexed.positions, UnorderedEquals<glm::dvec3>({glm::dvec3(0.0, 0.0, 0.0),
                                                                     glm::dvec3(1.0, 1.0, 1.0),
                                                                     glm::dvec3(3.0, 3.0, 3.0),
                                                                     glm::dvec3(4.0, 4.0, 4.0)}));
        CHECK_THAT(reindexed.uvs, UnorderedEquals<glm::dvec2>({glm::dvec2(0.0, 0.0),
                                                               glm::dvec2(1.0, 1.0),
                                                               glm::dvec2(3.0, 3.0),
                                                               glm::dvec2(4.0, 4.0)}));
        CHECK(reindexed.texture.has_value());
        CHECK(mat_equals(*reindexed.texture, *original.texture));
    };

    SECTION("copy overload") {
        SimpleMesh reindexed_mesh = mesh::reindex(mesh);
        run_checks(mesh, reindexed_mesh);
    }

    SECTION("inplace overload") {
        SimpleMesh original_mesh = mesh;
        mesh::reindex_inplace(mesh);
        SimpleMesh reindexed_mesh = std::move(mesh);
        run_checks(original_mesh, reindexed_mesh);
    }
}
