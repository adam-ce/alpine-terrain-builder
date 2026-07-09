#include "../catch2_helpers.h"

#include <numbers>

#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/utils.h"
#include "mesh/convert.h"
#include "mesh/cgal.h"

TEST_CASE("convert rountrip keeps precision") {
    SimpleMesh mesh;

    const double pi = std::numbers::pi_v<double>;
    REQUIRE((double)(float)pi != pi);

    mesh.positions.push_back(glm::dvec3(0, 0, 0));
    mesh.positions.push_back(glm::dvec3(pi, 0, 0));
    mesh.positions.push_back(glm::dvec3(0, pi, 0));

    mesh.triangles.push_back(glm::uvec3(0, 2, 1));

    const cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    SimpleMesh roundtrip_mesh = convert::to_simple_mesh(cgal_mesh);

    sort_and_normalize_triangles(mesh.triangles);
    sort_and_normalize_triangles(roundtrip_mesh.triangles);

    REQUIRE(roundtrip_mesh.positions == mesh.positions);
    REQUIRE(roundtrip_mesh.triangles == mesh.triangles);
}
