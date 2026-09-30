/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 * Copyright (C) 2024 Adam Celarek-Litofcenko
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

#include <numbers>

#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/normalize.h"
#include "mesh/convert.h"
#include "mesh/cgal.h"

TEST_CASE("convert rountrip keeps precision") {
    SimpleMesh mesh;

    const double pi = std::numbers::pi_v<double>;
    CHECK((double)(float)pi != pi);

    mesh.positions.push_back(glm::dvec3(0, 0, 0));
    mesh.positions.push_back(glm::dvec3(pi, 0, 0));
    mesh.positions.push_back(glm::dvec3(0, pi, 0));

    mesh.triangles.push_back(glm::uvec3(0, 2, 1));

    const cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    SimpleMesh roundtrip_mesh = convert::to_simple_mesh(cgal_mesh);

    mesh::sort_and_normalize_triangles(mesh.triangles);
    mesh::sort_and_normalize_triangles(roundtrip_mesh.triangles);

    CHECK(roundtrip_mesh.positions == mesh.positions);
    CHECK(roundtrip_mesh.triangles == mesh.triangles);
}
