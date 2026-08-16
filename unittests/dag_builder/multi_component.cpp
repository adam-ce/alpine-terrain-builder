#include <catch2/catch_test_macros.hpp>

#include <CGAL/Polygon_mesh_processing/self_intersections.h>

#include "clusterize.h"
#include "mesh/cgal.h"
#include "mesh/cleanup.h"
#include "mesh/connectivity/connected_components.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/convert.h"
#include "simplify.h"
#include "utils.h"

namespace {

void append_grid(
    SimpleMesh &mesh,
    const glm::dvec2 min,
    const glm::dvec2 max,
    const uint32_t resolution) {
    const uint32_t vertex_offset = mesh.vertex_count();
    for (uint32_t y = 0; y <= resolution; ++y) {
        for (uint32_t x = 0; x <= resolution; ++x) {
            const glm::dvec2 t(x / static_cast<double>(resolution), y / static_cast<double>(resolution));
            const glm::dvec2 position = glm::mix(min, max, t);
            mesh.positions.emplace_back(position, 0.0);
        }
    }

    const uint32_t row_size = resolution + 1;
    for (uint32_t y = 0; y < resolution; ++y) {
        for (uint32_t x = 0; x < resolution; ++x) {
            const uint32_t bottom_left = vertex_offset + y * row_size + x;
            const uint32_t bottom_right = bottom_left + 1;
            const uint32_t top_left = bottom_left + row_size;
            const uint32_t top_right = top_left + 1;
            mesh.triangles.emplace_back(bottom_left, bottom_right, top_right);
            mesh.triangles.emplace_back(bottom_left, top_right, top_left);
        }
    }
}

} // namespace

TEST_CASE("DAG simplification preserves point-touching mesh components") {
    SimpleMesh input;
    append_grid(input, glm::dvec2(-1.0), glm::dvec2(0.0), 8);
    append_grid(input, glm::dvec2(0.0), glm::dvec2(1.0), 8);
    REQUIRE(mesh::count_connected_components(input) == 2);

    const Clustering clustering = clusterize(std::move(input));
    const Clustering simplified = simplify(clustering);
    SimpleMesh result = clustering_to_mesh(simplified);
    mesh::remove_isolated_vertices(result);

    REQUIRE_FALSE(result.is_empty());
    CHECK(mesh::count_connected_components(result) == 2);
    CHECK(mesh::is_manifold(result));

    const std::vector<SimpleMesh> components = mesh::split_into_connected_components(result);
    REQUIRE(components.size() == 2);
    for (const SimpleMesh &component : components) {
        const cgal::Mesh cgal_mesh = convert::to_cgal_mesh(component);
        CHECK_FALSE(CGAL::Polygon_mesh_processing::does_self_intersect(cgal_mesh));
    }
}
