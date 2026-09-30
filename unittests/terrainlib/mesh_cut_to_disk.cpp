/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/common.hpp>

#include "../catch2_helpers.h"
#include "mesh/connectivity/topology.h"
#include "mesh/igl/cut_to_disk.h"
#include "mesh/validate.h"

TEST_CASE("cut_to_disk works on double-sided triangle") {
    mesh::Simple mesh({
        {0, 1, 2},
        {0, 2, 1}
    }, {
        {0, 0, 0},
        {1, 0, 0},
        {0, 1, 0},
    });

    const mesh::Topology old_topology = mesh::compute_topology(mesh);
    CHECK(old_topology.is_sphere());
    CHECK(!old_topology.is_disk(false));

    mesh::cut_to_disk(mesh);
    mesh::validate(mesh);
    const mesh::Topology new_topology = mesh::compute_topology(mesh);
    CHECK(!new_topology.is_sphere());
    CHECK(new_topology.is_disk(false));
}

TEST_CASE("cut_to_disk works on tetrahedron") {
    mesh::Simple mesh({
        {0, 1, 2},
        {2, 3, 0},
        {3, 2, 1},
        {1, 0, 3},
    }, {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
    });

    const mesh::Topology old_topology = mesh::compute_topology(mesh);
    CHECK(old_topology.is_sphere());
    CHECK(!old_topology.is_disk(false));

    mesh::cut_to_disk(mesh);
    mesh::validate(mesh);
    const mesh::Topology new_topology = mesh::compute_topology(mesh);
    CHECK(!new_topology.is_sphere());
    CHECK(new_topology.is_disk(false));
}

TEST_CASE("cut_to_disk works on tetrahedron triangles only") {
    std::vector<glm::uvec3> triangles = {
        {0, 1, 2},
        {2, 3, 0},
        {3, 2, 1},
        {1, 0, 3},
    };

    const mesh::Topology old_topology = mesh::compute_topology(triangles);
    CHECK(old_topology.is_sphere());
    CHECK(!old_topology.is_disk(false));

    mesh::cut_to_disk(triangles);
    const mesh::Topology new_topology = mesh::compute_topology(triangles);
    CHECK(!new_topology.is_sphere());
    CHECK(new_topology.is_disk(false));
}

TEST_CASE("cut_to_disk works on multiple tetrahedrons") {
    mesh::Simple mesh({
        {0, 1, 2},
        {2, 3, 0},
        {3, 2, 1},
        {1, 0, 3},

        {4, 5, 6},
        {6, 7, 4},
        {7, 6, 5},
        {5, 4, 7},
    }, {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},

        {2, 0, 0},
        {3, 0, 0},
        {3, 1, 0},
        {2, 1, 0},
    });

    const mesh::Topology old_topology = mesh::compute_topology(mesh);
    REQUIRE(old_topology.component_count() == 2);
    CHECK(old_topology.component(0).is_sphere());
    CHECK(old_topology.component(1).is_sphere());
    CHECK(!old_topology.is_disks(false));

    mesh::cut_to_disk(mesh);
    mesh::validate(mesh);
    const mesh::Topology new_topology = mesh::compute_topology(mesh);
    REQUIRE(new_topology.component_count() == 2);
    CHECK(!new_topology.component(0).is_sphere());
    CHECK(!new_topology.component(1).is_sphere());
    CHECK(new_topology.component(0).is_disk(false));
    CHECK(new_topology.component(1).is_disk(false));
    CHECK(new_topology.is_disks(false));
}
